# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import threading
import time
import weakref

from . import namedpipeclient
from . import proto
from . import smb
from . import utils
from .utils import logging
from .utils import tcpserver

__all__ = ("BridgeThread", )

logger = logging.get_internal_logger(__name__)


class _SocksClient(utils.NoDict):
    __slots__ = ("socks_token", "tcp_token", "_tcp_client_weak")

    def __init__(self, socks_token, tcp_client):
        assert isinstance(tcp_client, tcpserver.TcpServerClient)

        super().__init__()

        self.socks_token = socks_token
        self.tcp_token = tcp_client.token

        self._tcp_client_weak = weakref.ref(tcp_client)

    @property
    def tcp_client(self):
        return self._tcp_client_weak()

    # @property
    # def tcp_closed(self):
    #     tcp_client = self._tcp_client_weak()
    #     return tcp_client is None or tcp_client.is_closed


class BridgeThread(namedpipeclient.ProtoClientObserver,
                   tcpserver.TcpServerObserver):
    def __init__(self, *, smb_config, pipe_name, socks_bind_addrs,
                 proto_keep_alive=None, observers=()):
        assert isinstance(smb_config, smb.SmbConfig)

        namedpipeclient.ProtoClientObserver.__init__(self)
        tcpserver.TcpServerObserver.__init__(self)

        self._lock = threading.RLock()

        self._smbconfig = smb_config
        self._pipe_name = pipe_name

        self._socks_clients_by_socks = {}  # {socks_token: _SocksClient}
        self._socks_clients_by_tcp = {}  # {tcp_token: _SocksClient}
        self._pending_socks_disconnect_uids = set()

        self._proto_client = namedpipeclient.ProtoClientThread(
            smb_config=smb_config,
            pipe_name=pipe_name,
            observers=self,
            keep_alive=proto_keep_alive)

        self._socks_tcp_server = tcpserver.TcpServer(
            bind_addresses=socks_bind_addrs,
            observers=self)

        for obs in observers:
            self.register_observer(obs)

    def __del__(self):
        with contextlib.suppress(Exception):
            self._proto_client.unregister_observer(self)

    @property
    def smbconfig(self):
        return self._smbconfig

    @property
    def rhost_str(self):
        return self._smbconfig.rhost_str

    @property
    def socksrelay_bind_addresses(self):
        return self._socks_tcp_server.bind_addresses

    @property
    def protoclient_addr_str(self):
        return self._proto_client.addr_str

    @property
    def protoclient_connected(self):
        return self._proto_client.connected

    @property
    def socksrelay_running(self):
        return self._socks_tcp_server.is_alive()

    @property
    def all_connected(self):
        return self.protoclient_connected and self.socksrelay_running

    def register_observer(self, observer):
        self._proto_client.register_observer(observer)
        self._socks_tcp_server.register_observer(observer)

    def unregister_observer(self, observer):
        self._proto_client.unregister_observer(observer)
        self._socks_tcp_server.unregister_observer(observer)

    def unregister_all_observers(self):
        self._proto_client.unregister_all_observers(observer)
        self._socks_tcp_server.unregister_all_observers(observer)

    def protoclient_wait_for_connection(self, **kwargs):
        return self._proto_client.wait_for_connection(**kwargs)

    def protoclient_send(self, data):
        # if isinstance(data, proto.PacketBase):
        #     data = data.serialize()
        # elif not isinstance(data, bytes):
        #     raise ValueError("data")

        try:
            return self._proto_client.send(data)
        except Exception:
            return False

    def protoclient_reconnect(self):
        return self._proto_client.reconnect()

    def request_termination(self):
        self._socks_tcp_server.request_termination()
        self._proto_client.request_termination()

    def join(self, *, timeout=None):
        """
        Join internal threads.

        `request_termination` must be called first to initiate termination.

        Return `True` if threads were joind successfully or `False` on timeout,
        in case *timeout* is not `None`.
        """
        start = time.monotonic()

        while True:
            if (self._socks_tcp_server.join(timeout=0) and
                    self._proto_client.join(timeout=0)):
                return True

            # timeout?
            if timeout is not None and timeout >= 0:
                elapsed = time.monotonic() - start
                if elapsed >= timeout:
                    return False

            time.sleep(0.1)

    def _on_tcp_connected(self, tcp_server, tcp_client):
        if tcp_server is self._socks_tcp_server:
            with self._lock:
                socks_client = _SocksClient(
                    self._generate_socks_token(), tcp_client)

                # register SOCKS client locally
                self._socks_clients_by_socks[socks_client.socks_token] = socks_client
                self._socks_clients_by_tcp[socks_client.tcp_token] = socks_client

    def _on_tcp_recv(self, tcp_server, tcp_client):
        if tcp_server is self._socks_tcp_server:
            socks_client = self._find_socks_client_by_tcp(tcp_client.token)
            if socks_client is None:
                logger.warning(
                    "local SOCKS listener notified about an unregistered "
                    "client; ignoring...")
                return  # self._on_tcp_connected(tcp_server, tcp_client)

            # fetch packet(s) from this TCP client
            socks_packets = tcp_client.recv()

            # relay every packet to the server-side
            for socks_packet in socks_packets:
                packet = proto.SocksPacket(
                    socks_client.socks_token, socks_packet)
                packet = packet.serialize()

                # logger.debug(
                #     f"forwarding {len(packet)} bytes SOCKS from TCP to server")

                self._proto_client.send(packet)

    def _on_tcp_disconnected(self, tcp_server, tcp_client_token):
        if tcp_server is self._socks_tcp_server:
            socks_client = self._find_socks_client_by_tcp(tcp_client_token)
            if socks_client is None:
                return

            # notify server-side so that it can disconnect and remove the
            # related SOCKS link
            packet = proto.SocksDisconnectedPacket(socks_client.socks_token)
            self._proto_client.send(packet.serialize())

            with self._lock:
                self._pending_socks_disconnect_uids.add(packet.uid)

            # unregister this SOCKS link from local state (part 2)
            self._unregister_socks_client(socks_client)

    def _on_tcp_stopped(self, tcp_server, exception):
        if tcp_server is self._socks_tcp_server:
            if exception is not None:
                logger.critical(
                    "SOCKS TCP relay terminated with an error. "
                    "Shutting down...")

                # no need to raise *exception* here, it has been
                # logging.exception()'ed already by tcpserver

            self.request_termination()

    def _on_proto_connected(self, np_client):
        pass

    def _on_proto_disconnected(self, np_client):
        pass

    def _on_proto_recv_CHANNEL_SETUP(self, np_client, packet):
        logger.debug(
            f"weird, received a {packet.opcode.name} packet from named pipe "
            f"{np_client.addr_str}")

    def _on_proto_recv_CHANNEL_SETUP_ACK(self, np_client, packet):
        logger.debug(
            f"weird, received a {packet.opcode.name} packet from named pipe "
            f"{np_client.addr_str}")

    def _on_proto_recv_STATUS(self, np_client, packet):
        with self._lock:
            try:
                self._pending_socks_disconnect_uids.remove(packet.uid)
                can_print_msg = False
            except KeyError:
                can_print_msg = True

        if can_print_msg:
            logger.hinfo(
                f"received a STATUS response {packet.status.name} from "
                f"{np_client.addr_str} (uid: {packet.uid})")

    def _on_proto_recv_PING(self, np_client, packet):
        packet = proto.StatusPacket(proto.Status.OK, uid=packet.uid)
        packet = packet.serialize()
        np_client.send(packet)

    def _on_proto_recv_SOCKS(self, np_client, packet):
        assert isinstance(packet, proto.SocksPacket)

        if np_client is self._proto_client:
            socks_client = self._find_socks_client_by_socks(packet.socks_id)
            if socks_client is None:
                # logger.debug(
                #     f"server-side notified about unregistered SOCKS "
                #     f"link ID {packet.socks_id}; ignoring...")
                return

            # IMPORTANT: keep a ref to tcp_client since *socks_client* holds a
            # weakref only
            tcp_client = socks_client.tcp_client
            if tcp_client is None:
                return

            # logger.debug(
            #     f"forwarding {len(packet.socks_packet)} bytes SOCKS back to "
            #     f"TCP client")

            try:
                tcp_client.send(packet.socks_packet)
            except Exception:
                # if logger.isEnabledFor(logging.DEBUG):
                logger.exception("failed to relay SOCKS packet to TCP side")
                return

    def _on_proto_recv_SOCKS_CLOSE(self, np_client, packet):
        assert isinstance(packet, (
            proto.SocksClosePacket, proto.SocksDisconnectedPacket))

        if np_client is self._proto_client:
            socks_client = self._find_socks_client_by_socks(packet.socks_id)
            if socks_client is None:
                return

            with contextlib.suppress(KeyError):
                self._socks_tcp_server.close_client(socks_client.tcp_token)

            self._unregister_socks_client(socks_client)

    def _on_proto_recv_SOCKS_DISCONNECTED(self, np_client, packet):
        assert isinstance(packet, proto.SocksDisconnectedPacket)
        self._on_proto_recv_SOCKS_CLOSE(np_client, packet)

    def _on_proto_recv_UNINSTALL_SELF(self, np_client, packet):
        logger.debug(
            f"weird, received a {packet.opcode.name} packet from named pipe "
            f"server")

    def _generate_socks_token(self):
        with self._lock:
            while True:
                socks_token = proto.generate_socks_id()
                if socks_token not in self._socks_clients_by_socks:
                    return socks_token

    def _find_socks_client_by_socks(self, socks_token):
        with self._lock:
            try:
                return self._socks_clients_by_socks[socks_token]
            except KeyError:
                return None

    def _find_socks_client_by_tcp(self, tcp_token):
        with self._lock:
            try:
                return self._socks_clients_by_tcp[tcp_token]
            except KeyError:
                return None

    def _unregister_socks_client(self, socks_client):
        with self._lock:
            with contextlib.suppress(KeyError):
                del self._socks_clients_by_socks[socks_client.socks_token]

            with contextlib.suppress(KeyError):
                del self._socks_clients_by_tcp[socks_client.tcp_token]
