# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import threading
import time

from . import smb
from . import proto
from .utils import dispatcher
from .utils import logging

__all__ = ("ProtoClientThread", "ProtoClientObserver")


logger = logging.get_internal_logger(__name__)


class NamedPipeClientObserver(dispatcher.Observer):
    def __init__(self):
        super().__init__()

    def _on_namedpipe_connected(self, np_client):
        pass

    def _on_namedpipe_recv(self, np_client):
        pass

    def _on_namedpipe_disconnected(self, np_client):
        pass


class NamedPipeClientThread(dispatcher.Dispatcher):
    """
    An over-complicated named pipe client class that aims to compensate
    `impacket`'s lack of proper async I/O support.

    Instead of opening a single duplex pipe instance, two instances are opened,
    respectively for read-only and write-only operations. This allows for
    blocking I/O calls and eases the use of `impacket` at the cost of a more
    complex implementation both on client and server sides due to the handling
    of a handshake exchange at connection time and the pairing of dual
    connections per client.

    At connection time, the client-sude sends a `proto.ChannelSetupPacket`
    through each pipe instance to give the server-side a hint about how this
    instance - called *channel* in this context - is to be used. The server
    acknowledges the request by replying with a `proto.ChannelSetupAckPacket`.

    Once handshake exchange is done, channel settings are strictly honored by
    the server side.

    Also, note that even though the opened pipe instances are used in either
    read-only or write-only mode by this class, technically they are in duplex
    mode. This is because the so-called "channel setup" feature described above
    is offered by the application layer, not the transport layer.
    """

    def __init__(self, smbconfig, pipe_name, *, observers=()):
        super().__init__(
            dispatcher_raise_errors=False,
            dispatcher_logger=logger,
            observers=observers)

        assert isinstance(smbconfig, smb.SmbConfig)
        assert isinstance(pipe_name, str)

        # cleanup and normalize pipe name
        pipe_name = pipe_name.replace("/", "\\")
        while "\\\\" in pipe_name:
            pipe_name = pipe_name.replace("\\\\", "\\")
        pipe_name = "\\" + pipe_name.strip("\\")

        self._lock = threading.RLock()

        self._smbconfig = smbconfig
        self._pipe_name = pipe_name

        self._stop = False
        # self._read_event = threading.Event()
        self._write_event = threading.Event()

        self._read_queue = []
        self._write_queue = []

        self._pipe_read = None
        self._pipe_write = None

        self._thread_read = threading.Thread(
            target=self._read_loop,
            name=self.__class__.__name__ + "[READ]",
            daemon=True)

        self._thread_write = threading.Thread(
            target=self._write_loop,
            name=self.__class__.__name__ + "[WRITE]",
            daemon=True)

        self._thread_read.start()
        self._thread_write.start()
        time.sleep(0)  # yield

    @property
    def addr_str(self):
        return r"\\{}\pipe\{}".format(
            self._smbconfig.addr_str,
            self._pipe_name.lstrip("\\"))

    @property
    def rhost_str(self):
        return self._smbconfig.rhost_str

    @property
    def terminated(self):
        with self._lock:
            return self._thread_read is None and self._thread_write is None

    @property
    def connected(self):
        with self._lock:
            return (
                not self.terminated and
                self._pipe_read is not None and
                self._pipe_write is not None)

    def disconnect(self):
        self._disconnect(can_notify=True)

    def reconnect(self):
        return self._reconnect_impl()

    def wait_for_connection(self, *, timeout=6.0):
        if not (timeout is None or
                (isinstance(timeout, (int, float)) and timeout >= 0)):
            raise ValueError("timeout")

        start_time = time.monotonic()

        while not self.connected:
            if timeout is not None and time.monotonic() - start_time >= timeout:
                return False
            time.sleep(0.1)

        return True

    def read(self, *, bulk=False):
        with self._lock:
            if self._read_queue:
                if bulk:
                    result = self._read_queue
                    self._read_queue = []
                else:
                    result = self._read_queue.pop(0)

                # if not self._read_queue:
                #     self._read_event.clear()

                return result
            else:
                return [] if bulk else None

    def write(self, data):
        with self._lock:
            if self._stop:
                # raise RuntimeError("termination requested")
                return False
            # elif self._thread_write is None or not self._thread_write.is_alive():
            #     # raise RuntimeError("internal thread terminated")
            #     return False

            self._write_queue.append(data)
            self._write_event.set()

        return True

    def request_termination(self):
        self._stop = True
        self._disconnect(can_notify=False)

    def join(self, *, timeout=None):
        """
        Join internal threads.

        `request_termination` must be called first in order to request and to
        initiate threads termination.

        Return `True` if threads were joind successfully or `False` on timeout,
        in case *timeout* is not `None`.
        """
        # if self._thread_read is None and self._thread_write is None:
        #     return True

        # self._stop = True
        start = time.monotonic()

        while True:
            # if self._pipe_read is not None or self._pipe_write is not None:
            #     self._disconnect(can_notify=False)

            if self._thread_read is not None and not self._thread_read.is_alive():
                self._thread_read = None

            if self._thread_write is not None and not self._thread_write.is_alive():
                self._thread_write = None

            if self._thread_read is None and self._thread_write is None:
                if not (self._pipe_read is None and self._pipe_write is None):
                    self._disconnect(can_notify=False)
                break

            # if (self._pipe_read is None and
            #         self._pipe_write is None and
            #         self._thread_read is None and
            #         self._thread_write is None):
            #     break

            # timeout?
            if timeout is not None and timeout >= 0:
                elapsed = time.monotonic() - start
                if elapsed >= timeout:
                    return False

            time.sleep(0.1)

        return True

    def _read_loop(self):
        cls_name = self.__class__.__name__

        logger.debug(f"{cls_name}'s read thread started")

        self.reconnect()

        while True:
            with self._lock:
                if self._stop:
                    self._disconnect(can_notify=False)
                    break

                rpipe = self._pipe_read

            if rpipe is None or rpipe.closed:
                if rpipe is not None:
                    self._disconnect(can_notify=True)
                time.sleep(0.5)
                continue

            try:
                data = rpipe.read(timeout=1_000_000)
            except smb.SmbTimeoutError:
                rpipe = None  # release ref
                continue
            # except (ConnectionResetError, TypeError):
            #     # catch TypeError as well here due to a bug in impacket's
            #     # nmb.NetBIOSTCPSession.non_polling_read() when it raises a
            #     # NetBIOSError
            #     data = None
            except Exception:
                # logger.exception("failed to read on READ channel")
                data = None

            if not data:
                self._disconnect(can_notify=True)
                continue
            else:
                with self._lock:
                    self._read_queue.append(data)
                    # self._read_event.set()

                self.notify_observers("_on_namedpipe_recv", self)

        logger.debug(f"{cls_name}'s read thread gracefully stopped")

    def _write_loop(self):
        cls_name = self.__class__.__name__

        logger.debug(f"{cls_name}'s write thread started")

        while True:
            with self._lock:
                if self._stop:
                    break

                connected = self._pipe_write is not None and not self._pipe_write.closed
                has_data_to_write = len(self._write_queue) > 0

            if not connected:
                if self._pipe_write is not None:
                    self._disconnect(can_notify=True)
                time.sleep(0.5)
                continue

            if has_data_to_write or self._write_event.wait(timeout=1.0):
                self._write_loop__flush_queue()

        logger.debug(f"{cls_name}'s write thread gracefully stopped")

    def _write_loop__flush_queue(self):
        while True:
            with self._lock:
                if self._stop:
                    return

                wpipe = self._pipe_write
                if wpipe is None or wpipe.closed:
                    return

                if not self._write_queue:
                    self._write_event.clear()
                    return
                else:
                    data = self._write_queue.pop(0)
                    if not data:
                        return

            try:
                status = wpipe.write(data)
            except smb.SmbTimeoutError:
                status = False
                logger.warning(
                    f"failed to write {len(data)} bytes to {self.addr_str}: "
                    f"timeout")
            except ConnectionResetError:
                status = False
            except Exception as exc:
                status = False
                logger.warning(
                    f"failed to write {len(data)} bytes to {self.addr_str}: "
                    f"{exc}")

            if not status:
                with self._lock:
                    self._write_queue.insert(0, data)
                self._disconnect(can_notify=True)
                return

    def _disconnect(self, *, can_notify):
        with self._lock:
            was_disconnected = (
                self._pipe_read is None and
                self._pipe_write is None)

            if self._pipe_read is not None:
                with contextlib.suppress(Exception):
                    self._pipe_read.close()
                self._pipe_read = None

            if self._pipe_write is not None:
                with contextlib.suppress(Exception):
                    self._pipe_write.close()
                self._pipe_write = None

            # self._read_queue = []
            # self._write_queue = []

            # self._read_event.clear()
            # self._write_event.clear()

        if can_notify and not was_disconnected:
            self.notify_observers("_on_namedpipe_disconnected", self)

    def _reconnect_impl(self):
        self._disconnect(can_notify=False)

        logger.debug(f"connecting to {self.addr_str}...")

        # connect
        try:
            rpipe = smb.SmbNamedPipeDedicated(self._smbconfig, self._pipe_name)
            wpipe = smb.SmbNamedPipeDedicated(self._smbconfig, self._pipe_name)
        except Exception as exc:
            logger.warning(f"connection failed to {self.addr_str}: {exc}")
            return False

        # handshake (read-only pipe)
        try:
            client_id = self._reconnect__setup_channel(
                rpipe, proto.ChannelSetupFlag.READ)
        except Exception as exc:
            logger.warning(
                f"failed to setup read-only channel with {self.addr_str}: "
                f"{exc}")
            return False

        # handshake (write-only pipe)
        try:
            self._reconnect__setup_channel(
                wpipe, proto.ChannelSetupFlag.WRITE, client_id=client_id)
        except Exception as exc:
            logger.warning(
                f"failed to setup write-only channel with {self.addr_str}: "
                f"{exc}")
            return False

        # everything went smoothly
        with self._lock:
            # self._client_id = client_id
            self._pipe_read = rpipe
            self._pipe_write = wpipe

        self.notify_observers("_on_namedpipe_connected", self)

        return True

    def _reconnect__setup_channel(self, pipe, flags, *,
                                  client_id=0, io_timeout=3.0):
        # send setup request
        pipe.write(
            proto.ChannelSetupPacket(client_id, flags).serialize(),
            timeout=io_timeout)

        # wait for response
        read_size = (
            proto.HEADER_STRUCT.size +
            proto.ChannelSetupAckPacket.PAYLOAD_STRUCT.size)
        istream = proto.InputStream()
        while len(istream) < read_size:
            data = pipe.read(
                num_bytes=read_size-len(istream),
                timeout=io_timeout)

            if not data:
                raise Exception("pipe closed")

            istream.feed(data)

        # parse response
        packet = istream.flush_next_packet()
        if not isinstance(packet, proto.ChannelSetupAckPacket):
            raise Exception(
                f"unexpected connection handshake reply type: {type(packet)}")

        if client_id != 0 and packet.client_id != client_id:
            raise Exception(
                f"unexpected channel client id received from server (expected "
                f"{client_id}; got {packet.client_id}")

        return packet.client_id


class ProtoClientObserver(dispatcher.Observer):
    def __init__(self):
        super().__init__()

    def _on_proto_connected(self, np_client):
        pass

    def _on_proto_disconnected(self, np_client):
        pass

    def _on_proto_recv(self, np_client, packet):
        # if packet.opcode == proto.OpCode.CHANNEL_SETUP:
        #     logger.debug("weird, received a CHANNEL_SETUP packet from server")
        # elif packet.opcode == proto.OpCode.CHANNEL_SETUP_ACK:
        #     logger.debug("weird, received a CHANNEL_SETUP_ACK packet from server")
        # elif packet.opcode == proto.OpCode.STATUS:
        #     logger.debug(
        #         f"received STATUS {packet.status.name} from server "
        #         f"(uid #{packet.uid})")
        # elif packet.opcode == proto.OpCode.PING:
        #     packet = proto.StatusPacket(proto.Status.OK, uid=packet.uid)
        #     packet = packet.serialize()
        #     np_client.send(packet)
        # elif packet.opcode == proto.OpCode.UNINSTALL_SELF:
        #     logger.debug("weird, received a UNINSTALL_SELF packet from server")

        try:
            method = getattr(self, "_on_proto_recv_" + packet.opcode.name)
        except AttributeError:
            return

        method(np_client, packet)


class ProtoClientThread(dispatcher.Dispatcher, NamedPipeClientObserver):
    def __init__(self, *, smb_config, pipe_name, observers=(), keep_alive=None):
        if keep_alive is None:
            pass
        elif isinstance(keep_alive, (int, float)):
            if not keep_alive:
                keep_alive = None
            elif keep_alive < 0.1:
                keep_alive = 0.1
            elif keep_alive > 10.0:
                keep_alive > 10.0
        else:
            raise ValueError("keep_alive")

        NamedPipeClientObserver.__init__(self)
        dispatcher.Dispatcher.__init__(
            self,
            dispatcher_raise_errors=False,
            dispatcher_logger=logger,
            observers=observers)

        self._lock = threading.RLock()

        self._conn = NamedPipeClientThread(smb_config, pipe_name, observers=self)
        self._istream = proto.InputStream()

        self._stop = False
        self._recv_event = threading.Event()

        self._keepalive_delay = keep_alive
        self._keepalive_last_sent = 0
        self._keepalive_pending_resp = {}  # {uid: send_monotonic_time}

        self._thread = threading.Thread(
            target=self._logic_loop,
            name=self.__class__.__name__,
            daemon=True)

        self._thread.start()
        time.sleep(0)  # yield

    @property
    def rhost_str(self):
        return self._conn.rhost_str

    @property
    def addr_str(self):
        return self._conn.addr_str

    @property
    def connected(self):
        return self._conn.connected

    def wait_for_connection(self, *, timeout=6.0):
        return self._conn.wait_for_connection(timeout=timeout)

    def disconnect(self):
        self._conn.disconnect()

    def reconnect(self):
        return self._conn.reconnect()

    def request_termination(self):
        self._stop = True
        self._conn.request_termination()

    def join(self, *, timeout=None):
        """
        Join internal threads.

        `request_termination` must be called first in order to request and
        to initiate threads termination.

        Return `True` if threads were joind successfully or `False` on timeout,
        in case *timeout* is not `None`.
        """
        if self._thread is None:
            return True

        start = time.monotonic()

        self._conn.join(timeout=timeout)

        while True:
            if self._thread is None:
                break
            elif not self._thread.is_alive():
                self._thread = None
                break

            # timeout?
            if timeout is not None and timeout >= 0:
                elapsed = time.monotonic() - start
                if elapsed >= timeout:
                    return False

            time.sleep(0.1)

        return True

    def send(self, packet):
        if not self._stop and not self._conn.terminated:
            try:
                return self._conn.write(packet)
            except RuntimeError:
                return False

        return False

    def _on_namedpipe_connected(self, np_client):
        with self._lock:
            self._istream.clear()

        self.notify_observers("_on_proto_connected", self)

    def _on_namedpipe_disconnected(self, np_client):
        # with self._lock:
        #     self._istream.clear()

        self.notify_observers("_on_proto_disconnected", self)

    def _on_namedpipe_recv(self, np_client):
        self._recv_event.set()

    def _logic_loop(self):
        while True:
            if self._stop:
                break

            if self._keepalive_delay:
                if not self._recv_event.is_set():
                    self._send_keepalive()
                wait_timeout = self._keepalive_delay
            else:
                wait_timeout = 1.0

            if not self._recv_event.wait(timeout=wait_timeout):
                # if __debug__:
                #     if self._conn.connected:
                #         uid = proto.generate_uid()
                #         if self.send(proto.PingPacket(uid=uid).serialize()):
                #             logger.debug(f"sent PING #{uid}")
                continue

            if self._stop:
                break

            with self._lock:
                bulk = self._conn.read(bulk=True)

            if not bulk:
                continue

            self._istream.feed(bulk)

            while True:
                packet = self._istream.flush_next_packet()
                if packet is None:
                    self._recv_event.clear()
                    break
                elif not self._handle_keepalive_response(packet):
                    self.notify_observers("_on_proto_recv", self, packet)

    def _send_keepalive(self):
        if (self._keepalive_last_sent and
                time.monotonic() - self._keepalive_last_sent <
                    self._keepalive_delay):
            return False  # not yet

        while True:
            packet = proto.PingPacket()
            if packet.uid not in self._keepalive_pending_resp:
                break

        # not really a "send" time per-se since we've just pushed our packet
        # onto a queue but it is fine
        send_time = time.monotonic()

        self._keepalive_pending_resp[packet.uid] = send_time

        if not self.send(packet.serialize()):
            del self._keepalive_pending_resp[packet.uid]
            return False
        else:
            self._keepalive_last_sent = send_time
            return True

    def _handle_keepalive_response(self, packet):
        if isinstance(packet, proto.StatusPacket):
            value = self._keepalive_pending_resp.pop(packet.uid, None)
            if value is not None:
                if self._keepalive_pending_resp:
                    self._cleanup_pending_keepalives()

                if (self._keepalive_pending_resp and
                        logger.isEnabledFor(logging.DEBUG)):
                    logger.debug(
                        f"keep-alive response received "
                        f"({len(self._keepalive_pending_resp)} still pending)")

                return True

        return False  # not a response to a keep-alive packet

    def _cleanup_pending_keepalives(self):
        now = time.monotonic()
        to_delete = []

        for uid, send_time in self._keepalive_pending_resp.items():
            if now - send_time > 30.0:
                elapsed = now - send_time

                logger.debug(
                    f"cleaning up pending {elapsed} seconds old keep-alive "
                    f"response (uid: {uid})")

                to_delete.append(uid)

        for uid in to_delete:
            with contextlib.suppress(KeyError):
                del self._keepalive_pending_resp[uid]
