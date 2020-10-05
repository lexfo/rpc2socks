# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import io
import os
import socket
import time
import types
import weakref

import impacket.dcerpc.v5.dcomrt as impkt_dcomrt
import impacket.dcerpc.v5.transport as impkt_transport
import impacket.dcerpc.v5.srvs as impkt_srvs
import impacket.krb5.keytab as impkt_keytab
import impacket.smbconnection as impkt_smbconnection
import impacket.smb as impkt_smb
import impacket.smb3 as impkt_smb3
import impacket.nmb as impkt_nmb
import impacket.nt_errors as impkt_nt_errors

from impacket.dcerpc.v5.rpcrt import DCERPCException
from impacket.dcerpc.v5.epm import MSRPC_UUID_PORTMAP

from impacket.smb3structs import (
    # SMB dialects
    SMB2_DIALECT_002, SMB2_DIALECT_21, SMB2_DIALECT_30, SMB2_DIALECT_302,
    SMB2_DIALECT_311,

    # file attributes
    FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_COMPRESSED, FILE_ATTRIBUTE_DIRECTORY,
    FILE_ATTRIBUTE_ENCRYPTED, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL,
    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, FILE_ATTRIBUTE_OFFLINE,
    FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_REPARSE_POINT,
    FILE_ATTRIBUTE_SPARSE_FILE, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_TEMPORARY,
    FILE_ATTRIBUTE_INTEGRITY_STREAM, FILE_ATTRIBUTE_NO_SCRUB_DATA,

    # share access
    FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_DELETE,

    # create disposition
    FILE_SUPERSEDE, FILE_OPEN, FILE_CREATE, FILE_OPEN_IF, FILE_OVERWRITE,
    FILE_OVERWRITE_IF,

    # create options
    FILE_DIRECTORY_FILE, FILE_WRITE_THROUGH, FILE_SEQUENTIAL_ONLY,
    FILE_NO_INTERMEDIATE_BUFFERING, FILE_SYNCHRONOUS_IO_ALERT,
    FILE_SYNCHRONOUS_IO_NONALERT, FILE_NON_DIRECTORY_FILE,
    FILE_COMPLETE_IF_OPLOCKED, FILE_NO_EA_KNOWLEDGE, FILE_RANDOM_ACCESS,
    FILE_DELETE_ON_CLOSE, FILE_OPEN_BY_FILE_ID, FILE_OPEN_FOR_BACKUP_INTENT,
    FILE_NO_COMPRESSION, FILE_RESERVE_OPFILTER, FILE_OPEN_REPARSE_POINT,
    FILE_OPEN_NO_RECALL, FILE_OPEN_FOR_FREE_SPACE_QUERY,

    # file access mask / desired access
    FILE_READ_DATA, FILE_WRITE_DATA, FILE_APPEND_DATA, FILE_READ_EA,
    FILE_WRITE_EA, FILE_EXECUTE, FILE_READ_ATTRIBUTES, FILE_WRITE_ATTRIBUTES,
    DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, SYNCHRONIZE,
    ACCESS_SYSTEM_SECURITY, MAXIMUM_ALLOWED, GENERIC_ALL, GENERIC_EXECUTE,
    GENERIC_WRITE, GENERIC_READ)

from .utils import logging

__all__ = (
    "SmbError", "SmbTimeoutError",
    "DcomConnection", "SmbConfig", "SmbConnection", "SmbNamedPipeHandle",
    "SmbNamedPipeDedicated",
    "load_kerberos_keytab", "query_host_arch", "put_file", "delete_file")

logger = logging.get_internal_logger(__name__)


class SmbError(Exception):
    pass


class SmbTimeoutError(Exception):
    pass


class SmbConfig:
    def __init__(self, *, username="", password="", domain="", rhost_name="",
                 rhost_addr="", rport_smb=445, rport_rpc=135, hashes=None,
                 aes_key=None, do_kerberos=None, kdc_host=None):
        # config
        self.username = username
        self.password = password
        self.domain = domain
        self.rhost_name = rhost_name
        self.rhost_addr = rhost_addr
        self.rport_smb = rport_smb
        self.rport_rpc = rport_rpc
        # self.timeout = timeout  # see socket.socket.settimeout()
        self.lmhash = ""
        self.nthash = ""
        self.aes_key = aes_key
        self.do_kerberos = bool(do_kerberos)
        self.kdc_host = kdc_host

        if hashes is not None:
            self.lmhash, self.nthash = hashes.split(":")

    @property
    def rhost_str(self):
        return self.rhost_addr if self.rhost_addr else self.rhost_name

    @property
    def addr_str(self):
        if self.rhost_addr:
            return "{}[{}]".format(self.rhost_addr, self.rport_smb)
        else:
            return "{}[{}]".format(self.rhost_name, self.rport_smb)

    @property
    def dcom_target(self):
        return "{}[{}]".format(self.rhost_str, self.rport_rpc)

    def spawn_smb_connection(self):
        try:
            smbconn = impkt_smbconnection.SMBConnection(
                self.rhost_name, self.rhost_addr, sess_port=self.rport_smb,
                timeout=60, preferredDialect=None)
        except (impkt_nmb.NetBIOSTimeout, socket.timeout) as exc:
            raise SmbTimeoutError(str(exc))
        except impkt_smbconnection.SessionError as exc:
            if exc.getErrorCode() == impkt_nt_errors.STATUS_IO_TIMEOUT:
                raise SmbTimeoutError(str(exc))
            else:
                raise

        if self.do_kerberos:
            smbconn.kerberosLogin(
                self.username, self.password, self.domain, self.lmhash,
                self.nthash, self.aes_key, kdcHost=self.kdc_host,
                TGT=None, TGS=None)
        else:
            smbconn.login(
                self.username, self.password, self.domain, self.lmhash,
                self.nthash)

        logger.debug(
            f"connected to {self.addr_str} (SMB dialect "
            f"{smbconn.getDialect()}; MaxReadSize: "
            f"{smbconn._SMBConnection.getIOCapabilities()['MaxReadSize']})")

        return SmbConnection(smbconn)

    def spawn_dcom_connection(self, *, connect_timeout=6.0):
        # CAUTION: keep this one to False so to prevent impacket's
        # DCOMConnection class from launching a daemon thread (threading.Timer)
        # which will prevent interpreter from terminating
        oxid_resolver = False

        # CAUTION: changing RPC port number from default 135 is not well
        # supported by impacket. It is technically possible to specify the port
        # number as part of the *target* arg, however the information gets lost
        # on the way by impacket and it leads to either a KeyError exception, or
        # even a "Can't find a valid stringBinding to connect" error in case
        # connection must go through port mapping - e.g. typically a connection
        # test to a local VM.
        #
        # target = self.dcom_target  # <-- ideally we want this
        assert self.rport_rpc == 135
        target = self.rhost_str

        dcom_conn = impkt_dcomrt.DCOMConnection(
            target=target, username=self.username, password=self.password,
            domain=self.domain, lmhash=self.lmhash, nthash=self.nthash,
            aesKey=self.aes_key, oxidResolver=oxid_resolver,
            doKerberos=self.do_kerberos, kdcHost=self.kdc_host)

        dcom_conn.get_dce_rpc().get_rpc_transport().set_connect_timeout(
            connect_timeout)

        return DcomConnection(dcom_conn)


class DcomConnection:
    """Must be instantiated with `SmbConfig.spawn_dcom_connection`"""

    def __init__(self, dcom_conn):
        assert isinstance(dcom_conn, impkt_dcomrt.DCOMConnection)
        self._dcom_conn = dcom_conn

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()

    def __getattr__(self, name):
        return getattr(self._dcom_conn, name)

    @property
    def target(self):
        return self._dcom_conn.__target

    def close(self):
        self._dcom_conn.disconnect()


class SmbConnection:
    """Must be instantiated with `SmbConfig.spawn_smb_connection`"""

    # DEFAULT_PIPE_ACCESS_MASK = GENERIC_WRITE | GENERIC_READ | SYNCHRONIZE
    DEFAULT_PIPE_ACCESS_MASK = (
        FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA |
        FILE_WRITE_EA | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES |
        READ_CONTROL | SYNCHRONIZE)

    def __init__(self, smb_conn):
        assert isinstance(smb_conn, impkt_smbconnection.SMBConnection)

        self._smb_conn = smb_conn
        self._ipc_tree_id = None

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()

    def __getattr__(self, name):
        return getattr(self._smb_conn, name)

    @property
    def ipc_tree_id(self):
        if self._ipc_tree_id is None:
            self._ipc_tree_id = self._smb_conn.connectTree("IPC$")

        return self._ipc_tree_id

    @property
    def addr_str(self):
        rhost_addr = self._smb_conn.getRemoteHost()
        rhost_name = self._smb_conn.getRemoteName()
        rport = self._smb_conn._sess_port

        if rhost_addr:
            return "{}[{}]".format(rhost_addr, rport)
        else:
            return "{}[{}]".format(rhost_name, rport)

    def close(self):
        if self._ipc_tree_id is not None:
            try:
                self._smb_conn.disconnectTree(self._ipc_tree_id)
            except Exception:
                # impacket's exception handlers often raise exceptions :(
                # logger.exception("error occurred while closing SMB share point")
                pass
            finally:
                self._ipc_tree_id = None

        if self._smb_conn is not None:
            try:
                self._smb_conn.logoff()
            except Exception:
                # impacket's exception handlers often raise exceptions :(
                # logger.exception(
                #     "error occurred while logging off from SMB connection")
                pass

            try:
                self._smb_conn.close()
            except Exception:
                # impacket's exception handlers often raise exceptions :(
                # logger.exception("error occurred while closing SMB connection")
                pass
            finally:
                self._smb_conn = None

    def get_timeout(self):
        # a.k.a. the beauty of impacket's design consistency (part 1)

        attrs = ("get_timeout", "getTimeout", "__timeout", "_timeout")
        cls_name = self._smb_conn._SMBConnection.__class__.__name__

        for name in attrs:
            try:
                attr = getattr(self._smb_conn._SMBConnection, name)
            except AttributeError:
                continue

            if callable(attr):
                timeout = attr()
            else:
                timeout = attr

            if not isinstance(attr, (int, float)):
                raise RuntimeError(
                    f"unknown timeout value type for {cls_name}.{name}: "
                    f"{type(attr)}")

            return timeout

        raise RuntimeError(f"failed to get timeout value from class {cls_name}")

    def set_timeout(self, timeout):
        self._smb_conn.setTimeout(timeout)

    @contextlib.contextmanager
    def use_timeout(self, timeout):
        # a.k.a. the beauty of impacket's design consistency (part 2)

        if timeout is None:
            yield
        else:
            prev_timeout = self.get_timeout()
            self.set_timeout(timeout)

            try:
                yield
            finally:
                with contextlib.suppress(Exception):
                    self.set_timeout(prev_timeout)

    def list_shares(self):
        rpctransport = None
        dce = None

        try:
            rpctransport = impkt_transport.SMBTransport(
                remoteName=self._smb_conn.getRemoteHost(),  # getRemoteName(),
                remote_host=self._smb_conn.getRemoteHost(),
                dstport=self._smb_conn._sess_port,
                smb_connection=self._smb_conn,
                filename="\\srvsvc")

            dce = rpctransport.get_dce_rpc()
            dce.connect()
            dce.bind(impkt_srvs.MSRPC_UUID_SRVS)
            resp = impkt_srvs.hNetrShareEnum(dce, 1)
            resp = resp["InfoStruct"]["ShareInfo"]["Level1"]["Buffer"]
        except Exception as exc:
            raise SmbError(str(exc))
        finally:
            if dce is not None:
                with contextlib.suppress(Exception):
                    dce.disconnect()

            if rpctransport is not None:
                with contextlib.suppress(Exception):
                    rpctransport.disconnect()

        return resp

    # impacket's SMBConnection.listShares does not instantiate SMBTransport
    # properly
    listShares = list_shares

    def spawn_named_pipe(self, pipe_name, *,
                         access_mask=DEFAULT_PIPE_ACCESS_MASK,
                         open_timeout=5.0):
        timeout_point = time.monotonic() + open_timeout

        # wait for the remote named pipe to exist
        while True:
            # note: values returned by the different implementations of
            # waitNamedPipe() are not consistent
            try:
                res = self._smb_conn.waitNamedPipe(self.ipc_tree_id, pipe_name)
                if not (res is False or res is None or
                        (isinstance(res, int) and res == 0)):
                    break
            except (impkt_nmb.NetBIOSTimeout, socket.timeout) as exc:
                raise SmbTimeoutError(str(exc))
            except impkt_smbconnection.SessionError as exc:
                if exc.getErrorCode() == impkt_nt_errors.STATUS_IO_TIMEOUT:
                    raise SmbTimeoutError(str(exc))
                else:
                    raise

            if time.monotonic() > timeout_point:
                raise SmbTimeoutError(
                    "timeout while waiting for named pipe to be available")

            time.sleep(0.1)

        file_id = self._smb_conn.openFile(
            self.ipc_tree_id, pipe_name,
            desiredAccess=access_mask,
            creationOption=FILE_NON_DIRECTORY_FILE,  # | FILE_SYNCHRONOUS_IO_ALERT,
            fileAttributes=FILE_ATTRIBUTE_NORMAL)

        return SmbNamedPipeHandle(pipe_name, self, self.ipc_tree_id, file_id)


class SmbNamedPipeHandle:
    """Must be instantiated with `SmbConnection.spawn_named_pipe`"""

    def __init__(self, name, smbconn, tree_id, file_id):
        assert isinstance(smbconn, SmbConnection)

        self.name = name
        self.smbconn_weak = weakref.ref(smbconn)
        self.tree_id = tree_id
        self.file_id = file_id

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()

    @property
    def smbconn(self):
        smbconn = self.smbconn_weak()
        if smbconn is None:
            raise RuntimeError("smbconn object has been released")

        return smbconn

    @property
    def closed(self):
        return self.file_id is None

    def close(self):
        if self.file_id is not None:
            try:
                self.smbconn.closeFile(self.tree_id, self.file_id)
            except Exception:
                # impacket's exception handlers often raise exceptions :(
                # logger.exception("error occurred while closing named pipe")
                pass
            finally:
                self.file_id = None

    def read(self, num_bytes=None, *, timeout=None):
        if self.file_id is None:
            raise RuntimeError("pipe closed")

        smbconn = self.smbconn

        try:
            with smbconn.use_timeout(timeout):
                return smbconn.readNamedPipe(
                    self.tree_id, self.file_id, bytesToRead=num_bytes)
        except (impkt_nmb.NetBIOSTimeout, socket.timeout) as exc:
            raise SmbTimeoutError(str(exc))
        except ConnectionError:
            self.close()
            return b""
        except impkt_smbconnection.SessionError as exc:
            if exc.getErrorCode() == impkt_nt_errors.STATUS_IO_TIMEOUT:
                raise SmbTimeoutError(str(exc))
            elif exc.getErrorCode() in (
                    impkt_nt_errors.STATUS_PIPE_DISCONNECTED,
                    impkt_nt_errors.STATUS_END_OF_FILE,
                    impkt_nt_errors.STATUS_PIPE_BROKEN):
                self.close()
                return b""
            else:
                self.close()
                # raise SmbError("{} (error {:#x})".format(
                #     exc.getErrorString(), exc.getErrorCode()))
                return b""
        # except (impkt_smb.SessionError, impkt_smb3.SessionError) as exc:
        #     if exc.get_error_code() in (
        #             impkt_nt_errors.STATUS_END_OF_FILE,
        #             impkt_nt_errors.STATUS_PIPE_BROKEN):
        #         self.close()
        #         return b""
        #     else:
        #         raise SmbError(str(exc))
        except Exception as exc:
            # impacket's exception handlers often raise exceptions :(
            # logger.exception("error occurred while reading named pipe")
            self.close()
            raise

    def write(self, data, *, timeout=None):
        # CAUTION: we cannot even check the return value of the writeNamedPipe()
        # call because of impacket's inconsistency between `smb` and `smb3`
        # implementations

        if self.file_id is None:
            raise RuntimeError("pipe closed")

        smbconn = self.smbconn

        try:
            with smbconn.use_timeout(timeout):
                smbconn.writeNamedPipe(
                    self.tree_id, self.file_id, data)
        except (impkt_nmb.NetBIOSTimeout, socket.timeout) as exc:
            raise SmbTimeoutError(str(exc))
        # except ConnectionError:
        #     self.close()
        #     raise
        except impkt_smbconnection.SessionError as exc:
            if exc.getErrorCode() == impkt_nt_errors.STATUS_IO_TIMEOUT:
                raise SmbTimeoutError(str(exc))
            elif exc.getErrorCode() in (
                    impkt_nt_errors.STATUS_PIPE_DISCONNECTED,
                    impkt_nt_errors.STATUS_END_OF_FILE,
                    impkt_nt_errors.STATUS_PIPE_BROKEN):
                self.close()
                return False
            else:
                self.close()
                # raise SmbError("{} (error {:#x})".format(
                #     exc.getErrorString(), exc.getErrorCode()))
                return False
        except Exception as exc:
            # impacket's exception handlers often raise exceptions :(
            # logger.exception("error occurred while writing named pipe")
            self.close()
            raise

        return True


class SmbNamedPipeDedicated:
    """
    A remote named pipe over an `SmbConnection`.

    Can be instantiated directly.

    This class only exists due to intrinsic limitations of :mod:`impacket`,
    which disallow opening multiple concurrent instances of a same remote named
    pipe from a same SMB connection, forcing us to instantiate a dedicated
    `SmbConnection`.

    The constructor establishes the SMB connection and opens the remote named
    pipe instance.
    """

    def __init__(self, smbconfig, pipe_name):
        assert isinstance(smbconfig, SmbConfig)
        assert isinstance(pipe_name, str)

        self.smbconn = smbconfig.spawn_smb_connection()
        self.pipe = self.smbconn.spawn_named_pipe(pipe_name)

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()

        # order matters
        self.pipe = None
        self.smbconn = None

    def __getattr__(self, name):
        return getattr(self.pipe, name)

    @property
    def name(self):
        return self.pipe.name

    @property
    def addr_str(self):
        return "\\\\{}\\pipe\\{}".format(
            self.smbconn.addr_str,
            self.pipe.name.lstrip("\\"))

    def close(self):
        self.pipe.close()
        self.smbconn.close()


def load_kerberos_keytab(keytab_file, username, domain):
    result = types.SimpleNamespace()

    impkt_keytab.Keytab.loadKeysFromKeytab(
        keytab_file, username, domain, result)

    if hasattr(result, "aesKey"):
        return "aes_key", result.aesKey
    elif hasattr(result, "hashes"):
        return "hashes", result.hashes
    else:
        raise SmbError(
            f"{username}@{domain} not found in keytab: {keytab_file}")


def query_host_arch(host_name_or_addr, *, port=135, timeout=3.0):
    """
    Use host's DCERPC service (port 135) to query for a specific 64-bit only
    feature and check the response.

    Return `True` if host is 64-bit, `False` if host is 32-bit, or `None` in
    case of an error.

    Ref: ``impacket/examples/getArch.py``
    """
    # not really an SMB-related function, it probably should be moved to a
    # DCERPC-dedicated module

    NDR64_SYNTAX = ("71710533-BEBA-4937-8319-B5DBEF9CCC36", "1.0")

    host_name_or_addr = host_name_or_addr.strip()
    binding = f"ncacn_ip_tcp:{host_name_or_addr}[{port}]"

    transport = None
    dce = None
    is64bit = None

    try:
        transport = impkt_transport.DCERPCTransportFactory(binding)
        transport.set_connect_timeout(timeout)

        dce = transport.get_dce_rpc()
        dce.connect()

        try:
            dce.bind(MSRPC_UUID_PORTMAP, transfer_syntax=NDR64_SYNTAX)
            is64bit = True
        except DCERPCException as exc:
            if str(exc).find("syntaxes_not_supported") >= 0:
                is64bit = False  # 32-bit
            else:
                logger.exception("unhandled DCERPCException")
    except Exception:
        # logger.exception("failed to fetch host architecture")
        pass
    finally:
        if dce is not None:
            with contextlib.suppress(Exception):
                dce.disconnect()

        if transport is not None:
            with contextlib.suppress(Exception):
                transport.disconnect()

    # return a false value that is not *False* so that caller does not have to
    # mind the difference between the "32-bit" result and an error
    return is64bit


def put_file(*, fileish, smbconfig, share, destname):
    if hasattr(fileish, "read"):
        # CAUTION: this assumes file is opened in binary mode
        own_file = False
        fin = fileish
    elif isinstance(fileish, str) or hasattr(fileish, "__fspath__"):
        own_file = True
        fin = open(os.fspath(fileish), mode="rb")
    elif isinstance(fileish, bytes):
        own_file = True
        fin = io.BytesIO(fileish)
    else:
        raise ValueError("fileish")

    share = share.replace("/", "\\")
    share = share.strip("\\")

    destname = destname.replace("/", "\\")

    display_dest_addr = f"\\\\{smbconfig.addr_str}\\{share}\\{destname}"
    smbconn = None

    try:
        smbconn = smbconfig.spawn_smb_connection()
        smbconn.putFile(share, destname, fin.read)
    except OSError as exc:
        logger.error(f"failed to PUT file to {display_dest_addr}: {exc}")
        return False
    except Exception:
        logger.exception(f"failed to PUT file to {display_dest_addr}")
        return False
    finally:
        if smbconn is not None:
            with contextlib.suppress(Exception):
                smbconn.close()
            smbconn = None

        if own_file:
            fin.close()
        fin = None

    return True


def delete_file(*, smbconfig, share, destname):
    share = share.replace("/", "\\")
    share = share.strip("\\")

    destname = destname.replace("/", "\\")

    display_dest_addr = f"\\\\{smbconfig.addr_str}\\{share}\\{destname}"
    smbconn = None

    try:
        smbconn = smbconfig.spawn_smb_connection()
        smbconn.deleteFile(share, destname)
    except Exception:
        logger.exception(f"failed to delete {display_dest_addr}")
        return False
    finally:
        if smbconn is not None:
            with contextlib.suppress(Exception):
                smbconn.close()
            smbconn = None

    return True
