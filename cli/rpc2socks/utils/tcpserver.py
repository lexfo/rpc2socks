# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import collections
import contextlib
import ipaddress
import re
import select
import selectors
import socket
import threading
import time
import weakref

from . import _utils
from . import dispatcher
from . import logging

__all__ = (
    "AF_INET", "AF_INET6", "AF_UNSPEC",  # "AF_UNIX",
    "SOCK_STREAM", "SOCK_DGRAM",  # "SOCK_RAW",
    "IPPROTO_TCP", "IPPROTO_UDP",
    "TcpServer", "TcpServerObserver",
    "safe_close_socket", "string_to_netaddr_tuple", "string_to_addresses")


# address family
AF_INET = socket.AF_INET
AF_INET6 = socket.AF_INET6
AF_UNSPEC = socket.AF_UNSPEC
# AF_UNIX = socket.AF_UNIX

# socket types
SOCK_STREAM = socket.SOCK_STREAM
SOCK_DGRAM = socket.SOCK_DGRAM
# SOCK_RAW = socket.SOCK_RAW

# socket protocols
IPPROTO_TCP = socket.IPPROTO_TCP
IPPROTO_UDP = socket.IPPROTO_UDP


logger = logging.get_internal_logger(__name__)


class NetAddr(_utils.NoDict):
    """Meant to be instantiated by `string_to_addresses`"""

    __slots__ = ("_family", "_socktype", "_sockproto", "_sockaddr")

    def __init__(self, family, socktype, sockproto, sockaddr):
        super().__init__()

        if family not in (AF_INET, AF_INET6):
            raise ValueError("family")

        if not isinstance(sockaddr, tuple):
            raise ValueError("sockaddr")

        self._family = family
        self._socktype = socktype
        self._sockproto = sockproto
        self._sockaddr = sockaddr

    def __str__(self):
        if self._family == AF_INET:
            return "{}:{}".format(
                "*" if not self._sockaddr[0] else self._sockaddr[0],
                self._sockaddr[1])
        elif self._family == AF_INET6:
            return "[{}]:{}".format(
                "*" if not self._sockaddr[0] else self._sockaddr[0],
                self._sockaddr[1])
        else:
            raise NotImplementedError

    @property
    def family(self):
        return self._family

    @property
    def host(self):
        if self._family in (AF_INET, AF_INET6):
            return self._sockaddr[0]
        else:
            raise NotImplementedError

    @property
    def port(self):
        if self._family in (AF_INET, AF_INET6):
            return self._sockaddr[1]
        else:
            raise NotImplementedError

    @property
    def socktype(self):
        return self._socktype

    @property
    def sockproto(self):
        return self._sockproto

    @property
    def sockaddr(self):
        return self._sockaddr


class TcpNetAddr(NetAddr):
    __slots__ = ()

    def __init__(self, family, sockaddr):
        super().__init__(family, SOCK_STREAM, IPPROTO_TCP, sockaddr)


class TcpServerClient:
    """
    Represents a remote client connected to `TcpServer`.

    Used internally, instantiated by `TcpServer`.
    """

    # __slots__ = (
    #     "_token", "_sock", "_raddr", "_output_queue", "_input_queue",
    #     "_must_update_selector", "_parent_weak", "_parent_lock", "data_bag")

    def __init__(self, sock, raddr, parent, parent_lock):
        super().__init__()

        self._token = id(self)
        self._sock = sock
        self._raddr = raddr
        self._recv_buffer = bytearray(64 * 1024)
        self._queues_lock = threading.RLock()
        self._output_queue = collections.deque()  # thread-safe
        self._input_queue = collections.deque()  # thread-safe
        self._must_update_selector = True

        self._parent_weak = weakref.ref(parent)
        self._parent_lock = parent_lock

        self.data_bag = None

    @property
    def token(self):
        return self._token

    @property
    def parent(self):
        return self._parent_weak()  # may return None

    @property
    def is_closed(self):
        return self._sock is None  # or self._parent_weak() is None

    @property
    def sock(self):
        return self._sock

    @property
    def fileno(self):
        if self._sock is not None:
            fd = self._sock.fileno()
            if fd >= 0:
                return fd

        return None

    @property
    def remote_addr(self):
        return self._raddr

    def recv(self):
        with self._queues_lock:
            if not self._input_queue:
                return type(self._input_queue)()
            else:
                packets = self._input_queue
                self._input_queue = type(self._input_queue)()
                return packets

    def send(self, data):
        with self._queues_lock:
            if self.is_closed:
                return False

            # output queue goes from empty to non-empty so the socket
            # selector must be updated
            if not self._output_queue:
                self._must_update_selector = True
                self._notify_parent()

            self._output_queue.append(data)

            return True

    def _update_selector(self, sel):
        if self._sock is None:
            self._must_update_selector = False
            return

        if sel is None:
            return

        with self._parent_lock:
            if self._must_update_selector:
                flags = selectors.EVENT_READ

                if self._output_queue:
                    flags |= selectors.EVENT_WRITE

                try:
                    sel.modify(self._sock, flags, data=self)
                except KeyError:
                    sel.register(self._sock, flags, data=self)

                self._must_update_selector = False

    def _notify_parent(self):
        parent = self.parent
        if parent is not None:
            try:
                parent._ioint_event.set()
            except Exception:
                pass

    def _safe_close(self, sel):
        with self._parent_lock:
            with self._queues_lock:
                if self._sock is not None:
                    safe_close_socket(self._sock, sel=sel)
                    self._sock = None
                    self._output_queue.clear()
                    self._must_update_selector = False

    def _recv_impl(self, sel):
        loop_idx = 0
        view = memoryview(self._recv_buffer)

        while True:
            sock = self._sock
            if sock is None:
                return False

            try:
                received = sock.recv_into(view)
            except (BlockingIOError, InterruptedError):
                return False
            except Exception:
                # logger.exception(f"failed to recv() from TCP client socket")
                received = None  # close

            if not received:
                self._safe_close(sel)
                return False

            data = view[:received].tobytes()

            with self._queues_lock:
                self._input_queue.append(data)

            if loop_idx >= 1:
                return True
            else:
                loop_idx += 1

                if select.select((sock, ), (), (), 0)[0]:
                    continue

                return True

    def _send_impl(self, sel):
        sock = self._sock
        if sock is None:
            return 0 == len(self._output_queue)

        with self._queues_lock:
            if not self._output_queue:
                return True

            output_queue = self._output_queue
            self._output_queue = type(self._output_queue)()

        while True:
            try:
                data = output_queue.popleft()
            except IndexError:
                break

            try:
                sent = sock.send(data)
            except InterruptedError:
                output_queue.appendleft(data)
                break
            except BlockingIOError as exc:
                try:
                    sent = exc.characters_written
                except AttributeError:
                    sent = 0

                if not sent:
                    output_queue.appendleft(data)
                    # logger.warning("aborted TCP send due to BlockingIOError")  # TEST
                    break
            except Exception:
                # logger.exception(f"failed to send() to TCP client socket")
                sent = 0  # close

            if sent == 0:
                output_queue.appendleft(data)
                self._safe_close(sel)
                break
            elif sent < len(data):
                data = data[sent:]
                output_queue.appendleft(data)
                # logger.warning("partial TCP send")  # TEST
                break

        # restore output queue in case it could not be flushed completely
        if output_queue:
            with self._queues_lock:
                output_queue.extend(self._output_queue)
                self._output_queue = output_queue

            return False
        else:
            self._must_update_selector = True
            return True


class TcpServerObserver(dispatcher.Observer):
    """
    A model of *observer* class for `TcpServer`.

    Implementor does not *have to* derive from this class.
    """

    def __init__(self):
        super().__init__()

    def _on_tcp_connected(self, tcp_server, tcp_client):
        """
        A client just established a connection.

        *tcp_server* is the `TcpServer` object.

        *tcp_client* is the `TcpServerClient` object.
        """
        pass

    def _on_tcp_recv(self, tcp_server, tcp_client):
        """
        A client just received some data.

        *tcp_server* is the `TcpServer` object.

        *tcp_client* is the `TcpServerClient` object.
        """
        pass

    def _on_tcp_disconnected(self, tcp_server, tcp_client_token):
        """
        A client just disconnected.

        *tcp_server* is the `TcpServer` object.

        *tcp_client_token* is `TcpServerClient.token` value (`int`).
        """
        pass

    def _on_tcp_stopped(self, tcp_server, exception):
        """
        `TcpServer` instance's own thread terminated gracefully or upon error.

        *exception* is either `None` or an `Exception`-derived object.
        """
        pass


class TcpServer(dispatcher.Dispatcher):
    """
    A generic TCP server following the *reactor* pattern, having its own single
    thread.

    It differs from standard module `socketserver` in that it only creates a
    single thread regardless of the number of currently connected clients,
    whereas `socketserver.ThreadingTCPServer` creates a thread per connected
    client.

    This class relies on `selectors` to handle I/O events.

    Internally, a pair of connected sockets - called "monitor sockets" here - is
    created so that a ``select()`` call can be *interrupted* in case of data to
    be sent on a socket.
    """

    DEFAULT_BIND = (TcpNetAddr(AF_INET, ("localhost", 8888)), )
    SELDATA_FOR_LISTENSOCK = None
    SELDATA_FOR_MONITORSOCK = False

    def __init__(self, *, bind_addresses=DEFAULT_BIND,
                 allow_reuse_address=False, request_queue_size=25,
                 observers=()):
        super().__init__(
            dispatcher_raise_errors=False,
            dispatcher_logger=logger,
            observers=observers)

        listening_sockets = []
        final_bind_addresses = []

        if not isinstance(bind_addresses, (tuple, list)):
            raise ValueError("bind_addresses not a tuple/list")

        if not bind_addresses:
            raise ValueError("empty bind_addresses tuple/list")

        for addr in bind_addresses:
            sock = socket.socket(
                family=addr.family, type=addr.socktype, proto=addr.sockproto)

            if allow_reuse_address:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            sock.bind(addr.sockaddr)

            listening_sockets.append(sock)
            final_bind_addresses.append(addr)

        # options
        self._request_queue_size = request_queue_size

        # low-level state
        self._lock = threading.RLock()
        self._stop = False
        self._ioint_event = threading.Event()
        self._sel = selectors.DefaultSelector()
        self._bind_addresses = final_bind_addresses
        self._listening_sockets = listening_sockets
        self._monitor_sockets = None
        self._clients = {}  # {client.token: client}

        # i/o loop thread
        self._thread_io = threading.Thread(
            target=self._iothread_entry,
            name=self.__class__.__name__ + "[io]",
            daemon=True)

        # "i/o interrupt" thread
        self._thread_ioint = threading.Thread(
            target=self._iointthread_entry,
            name=self.__class__.__name__ + "[ioint]",
            daemon=True)

        self._thread_io.start()
        self._thread_ioint.start()
        time.sleep(0)  # yield

    def __del__(self):
        with contextlib.suppress(BaseException):
            self._close_all()

    @property
    def bind_addresses(self):
        return self._bind_addresses

    def request_termination(self):
        """Request this server's own I/O handling thread to leave gracefully."""
        with self._lock:
            self._stop = True
            self._ioint_event.set()

    def is_alive(self):
        """Check if this server's own I/O handling thread is still running."""
        with self._lock:
            if self._thread_io is not None:
                if self._thread_io.is_alive():
                    return True

                self._thread_io = None

            return False

    def join(self, *, timeout=None):
        """
        Join internal threads.

        `request_termination` must be called first in order to request and to
        initiate threads termination.

        Return `True` if threads were joind successfully or `False` on timeout,
        in case *timeout* is not `None`.
        """
        start = time.monotonic()

        # self.request_termination()

        with self._lock:
            if self._thread_io is None and self._thread_ioint is None:
                return True

            th_io = self._thread_io
            th_ioint = self._thread_ioint

        while True:
            if th_io is not None and not th_io.is_alive():
                th_io = None

            if th_ioint is not None and not th_ioint.is_alive():
                th_ioint = None

            if th_io is None and th_ioint is None:
                with self._lock:
                    self._thread_io = None
                    self._thread_ioint = None

                return True

            # timeout?
            if timeout is not None and timeout >= 0:
                elapsed = time.monotonic() - start
                if elapsed >= timeout:
                    return False

            time.sleep(0.1)

    def send_to_client(self, client_token, data):
        """
        Send *data* (`bytes` object) to the remote client specified by its
        *client_token*.

        Return `True` if data has been enqueued, or `False` if socket is
        disconnected.

        Raise `KeyError` if client is not found.
        """
        with self._lock:
            return self._clients[client_token].send(data)

    def close_client(self, client_token):
        """
        Close connection to the remote client specified by its *client_token*.

        Raise `KeyError` if client is not found.
        """
        with self._lock:
            self._clients[client_token]._safe_close(self._sel)

    def _iointthread_entry(self):
        while True:
            try:
                self._iointthread_impl()
                if self._stop:
                    break
            except Exception as exc:
                logger.exception(
                    f"exception raised in {self.__class__.__name__}'s "
                    f"[i/o interrupt] thread")
                time.sleep(2.0)

    def _iointthread_impl(self):
        while True:
            with self._lock:
                if self._stop:
                    break

            if self._ioint_event.wait():
                if self._stop:
                    break

                time.sleep(0)  # yield

                if self._interrupt_select():
                    self._ioint_event.clear()
                else:
                    time.sleep(0.25)

    def _iothread_entry(self):
        try:
            self._iothread_impl()
            exception = None
        except Exception as exc:
            exception = exc
            logger.exception(
                f"exception raised in {self.__class__.__name__}'s i/o thread")

        self.notify_observers("_on_tcp_stopped", self, exception)

    def _iothread_impl(self):
        # listen for incoming connections
        with self._lock:
            for sock in self._listening_sockets:
                sock.listen(self._request_queue_size)
                sock.setblocking(False)
                self._sel.register(
                    sock, selectors.EVENT_READ,
                    data=self.SELDATA_FOR_LISTENSOCK)

        # i/o loop
        while True:
            with self._lock:
                # should we leave?
                if self._stop:
                    break

                # ensure *monitor sockets* are created
                if self._monitor_sockets is None:
                    self._create_monitor_sockets()

            # update the selector if needed
            for client in self._clients.values():
                if client._must_update_selector:
                    client._update_selector(self._sel)

            io_events = self._sel.select(timeout=1.0)
            # logger.debug(f"{self} select(): got {len(io_events)} events")

            for evt_key, evt_flags in io_events:
                if self._stop:
                    break

                if evt_key.data is self.SELDATA_FOR_LISTENSOCK:
                    # event on a listening socket
                    assert evt_key.fileobj in self._listening_sockets
                    self._on_accept(evt_key.fileobj)
                elif evt_key.data is self.SELDATA_FOR_MONITORSOCK:
                    # event on a monitor socket
                    assert evt_key.fileobj in self._monitor_sockets
                    self._on_monitor_io(evt_key, evt_flags)
                else:
                    # event on a client connection
                    self._on_client_io(evt_key, evt_flags)

        # unregister, shutdown and close every socket
        self._close_all()

    def _on_accept(self, listen_sock):
        sock, raddr = listen_sock.accept()
        sock.setblocking(False)

        client = TcpServerClient(sock, raddr, self, self._lock)

        with self._lock:
            assert client._must_update_selector
            client._update_selector(self._sel)
            self._clients[client.token] = client

        self.notify_observers("_on_tcp_connected", self, client)

    def _on_monitor_io(self, evt_key, evt_flags):
        sock = evt_key.fileobj

        if evt_flags & selectors.EVENT_READ:
            try:
                data = sock.recv(256)
                # TEST
                # if data and len(data) > 5:
                #     logger.warning(f"monitor socket received {len(data)} bytes")
                # TESTEND
            except InterruptedError:
                return

            if not data:
                logger.warning(f"duh?! reopening TCP monitor socket...")

                with self._lock:
                    self._close_monitor_sockets()
                    self._create_monitor_sockets()

    def _on_client_io(self, evt_key, evt_flags):
        client = evt_key.data
        data_received = False

        if client.is_closed:
            return  # duh?!

        assert evt_key.fileobj is client.sock

        if evt_flags & selectors.EVENT_READ:
            data_received = client._recv_impl(self._sel)

        if not client.is_closed and (evt_flags & selectors.EVENT_WRITE):
            client._send_impl(self._sel)

        if client.is_closed:
            self.notify_observers("_on_tcp_disconnected", self, client.token)
            with self._lock:
                del self._clients[client.token]
        elif data_received:
            self.notify_observers("_on_tcp_recv", self, client)

    def _create_monitor_sockets(self):
        with self._lock:
            if self._monitor_sockets is not None:
                self._close_monitor_sockets()

            try:
                self._monitor_sockets = socket.socketpair()
            except Exception:
                logger.exception(
                    f"{self.__class__.__name__} failed to create a pair of "
                    f"monitor sockets pair")

            for sock in self._monitor_sockets:
                self._sel.register(
                    sock, selectors.EVENT_READ,
                    data=self.SELDATA_FOR_MONITORSOCK)

    def _close_monitor_sockets(self):
        with self._lock:
            if self._monitor_sockets is None:
                return

            safe_close_socket(self._monitor_sockets[0], sel=self._sel)
            safe_close_socket(self._monitor_sockets[1], sel=self._sel)

            self._monitor_sockets = None

    def _interrupt_select(self):
        with self._lock:
            if self._monitor_sockets is None:
                self._create_monitor_sockets()

            if self._monitor_sockets is not None:
                try:
                    self._monitor_sockets[0].send(b"\x00")
                    return True
                except InterruptedError:
                    pass
                except Exception:
                    logger.exception("failed to write to monitor socket")

            return False

    def _close_all(self):
        with self._lock:
            if self._listening_sockets:
                for sock in self._listening_sockets:
                    safe_close_socket(sock, sel=self._sel)

                self._listening_sockets = []

            if self._clients:
                for client in self._clients.values():
                    client._safe_close(self._sel)

                self._clients = {}

            self._close_monitor_sockets()

            if self._sel is not None:
                with contextlib.suppress(Exception):
                    self._sel.close()

                self._sel = None


def safe_close_socket(sock, *, sel=None):
    """
    Call `socket.shutdown` (with `socket.SHUT_RDWR`) then `socket.close` on
    socket *sock* without raising any `Exception`.

    *sel* is optional and must be a `selectors.BaseSelector` derived object if
    specified, in which case call ``sel.unregister(sock)`` will be made.
    """
    if sock is not None:
        if sel is not None:
            with contextlib.suppress(Exception):
                sel.unregister(sock)

        with contextlib.suppress(Exception):
            sock.shutdown(socket.SHUT_RDWR)

        with contextlib.suppress(Exception):
            sock.close()


def string_to_netaddr_tuple(addr_string):
    af = None

    # IPv6?
    if af is None:
        rem = re.fullmatch(
            r"^\[([0-9a-fA-F\:]+)\](?:\:(\d{1,5}))?$",  # loosy check
            addr_string, re.A)
        if rem:
            af = AF_INET6
            addr = rem.group(1)
            port = rem.group(2)  # may be None

            try:
                addr = ipaddress.ip_address(addr)
            except ValueError:
                raise ValueError(f"invalid IPv6 network address: {addr_string}")

    # IPv4?
    if af is None:
        # regex is loosy on purpose
        # address will be validated by ipaddress
        rem = re.fullmatch(
            # r"^(\d{1,3}(?:\.\d{1,3}){1,3})(?:\:\d{1,5})?$",
            r"^([\d\.]+)(?:\:\d{1,5})?$",
            addr_string, re.A)

        if rem:
            af = AF_INET
            addr = rem.group(1)
            port = rem.group(2)  # may be None

            try:
                addr = ipaddress.ip_address(addr)
            except ValueError:
                raise ValueError(f"invalid IPv4 network address: {addr_string}")

    # "<name|empty>[:port]"?
    if af is None:
        af = AF_UNSPEC
        addr = addr_string.rsplit(":", maxsplit=1)

        if len(addr) > 1:
            addr, port = addr[0], addr[1]
        else:
            addr, port = addr[0], None

        addr = addr.strip()

        if addr in ("::", "[::]"):
            af = AF_INET6
            addr = ""  # make it compatible with socket module
        elif addr == "*":
            addr = ""  # make it compatible with socket module
        elif ":" in addr:
            raise ValueError(f"invalid host address: {addr_string}")

    # malformed input
    if af is None:
        raise ValueError(f"invalid host address: {addr_string}")

    if port is not None:
        try:
            port = int(port.strip())
        except ValueError:
            raise ValueError(f"invalid port number from address: {addr_string}")

    # [paranoid] enforce *af* if needed
    if isinstance(addr, ipaddress.IPv4Address):
        af = AF_INET
        addr = str(addr)
    elif isinstance(addr, ipaddress.IPv6Address):
        af = AF_INET6
        addr = str(addr)

    return (af, addr, port)


def string_to_addresses(addr_string, *,
                        passive=False, prefer_ipv4=False,
                        socktype=SOCK_STREAM, sockproto=IPPROTO_TCP,
                        gai_flags=0):
    if passive:
        gai_flags |= socket.AI_PASSIVE
    elif gai_flags & socket.AI_PASSIVE:
        raise ValueError(
            "gai_flags contains AI_PASSIVE but passive option is disabled")

    family, host, port = string_to_netaddr_tuple(addr_string)
    is_any = host == ""

    if not port:
        raise ValueError(
            f"port number invalid or missing in network address: {addr_string}")

    # the ANY (wildcard) address is a special case
    if is_any:
        if not passive:
            raise ValueError(
                f"ANY not supported for non-passive network addresses: "
                f"{addr_string}")

        # check for IPv4 if caller asks for it
        if family == AF_UNSPEC:
            # may raise socket.gaierror
            gaires = socket.getaddrinfo(
                host, port, 0,
                type=socktype, proto=sockproto, flags=gai_flags)

            # honor IPv4 preference if it is supported by the running platform
            if prefer_ipv4:
                for ai in gaires:
                    if ai[0] == AF_INET:
                        family = AF_INET
                        break

            # choose whatever family comes first
            if family == AF_UNSPEC and gaires:
                family = gaires[0][0]

        if family != AF_UNSPEC:
            # socket.bind() deals fine with "" host for both AF_INET and
            # AF_INET6 families
            netaddr = NetAddr(
                family=family, socktype=socktype, sockproto=sockproto,
                sockaddr=("", port))  # this works for AF_INET6 too in THAT case

            return [netaddr]

    # may raise socket.gaierror
    gaires = socket.getaddrinfo(
        host, port, family,
        type=socktype, proto=sockproto, flags=gai_flags)

    addresses_ipv4 = []
    addresses_tail = []

    for ai in gaires:
        netaddr = NetAddr(
            family=ai[0], socktype=ai[1], sockproto=ai[2], sockaddr=ai[4])

        if prefer_ipv4 and netaddr.family == AF_INET:
            # so that IPv4 addresses get pushed to the top of the final list
            addresses_ipv4.append(netaddr)
        else:
            addresses_tail.append(netaddr)

    return addresses_ipv4 + addresses_tail
