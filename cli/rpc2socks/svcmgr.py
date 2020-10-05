# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import time

import impacket.dcerpc.v5.scmr as impkt_scmr
import impacket.dcerpc.v5.transport as impkt_transport

from . import smb
from . import utils

from .utils import logging

__all__ = ("ServiceManager", )

logger = logging.get_internal_logger(__name__)


class ServiceManager:
    def __init__(self, *, smbconfig):
        assert isinstance(smbconfig, smb.SmbConfig)

        self._smbconfig = smbconfig
        self._smbconn = None
        self._smbtransport = None
        self._rpcsvcctl = None
        self._svcmgr = None

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()

    def close(self):
        if self._svcmgr is not None:
            if self._rpcsvcctl is not None:
                with contextlib.suppress(Exception):
                    impkt_scmr.hRCloseServiceHandle(
                        self._rpcsvcctl, self._svcmgr)
            self._svcmgr = None

        if self._rpcsvcctl is not None:
            with contextlib.suppress(Exception):
                self._rpcsvcctl.disconnect()
            self._rpcsvcctl = None

        if self._smbtransport is not None:
            with contextlib.suppress(Exception):
                self._smbtransport.disconnect()
            self._smbtransport = None

        if self._smbconn is not None:
            with contextlib.suppress(Exception):
                self._smbconn.close()
            self._smbconn = None

    def open(self):
        try:
            self._open_impl()
        except Exception:
            self.close()
            raise

    def create_service(self, name, display_name, cmdline, *, start=True):
        if self._svcmgr is None:
            raise RuntimeError("ServiceManager must be open() first")

        svc_handle = None

        try:
            # try to open existing service first
            try:
                resp = impkt_scmr.hROpenServiceW(
                    self._rpcsvcctl, self._svcmgr, name + "\x00")
                svc_handle = resp["lpServiceHandle"]
            except Exception as exc:
                svc_handle = None

                if str(exc).find("ERROR_SERVICE_DOES_NOT_EXIST") >= 0:
                    pass
                else:
                    raise

            # create service if needed
            if svc_handle is None:
                resp = impkt_scmr.hRCreateServiceW(
                    self._rpcsvcctl, self._svcmgr,
                    name + "\x00",
                    display_name + "\x00",
                    lpBinaryPathName=cmdline + "\x00",
                    dwStartType=impkt_scmr.SERVICE_DEMAND_START)

                svc_handle = resp["lpServiceHandle"]

            assert svc_handle is not None

            # start service
            try:
                impkt_scmr.hRStartServiceW(self._rpcsvcctl, svc_handle)
            except Exception as exc:
                if str(exc).find("ERROR_SERVICE_ALREADY_RUNNING") < 0:
                    raise
        finally:
            if svc_handle is not None:
                with contextlib.suppress(Exception):
                    impkt_scmr.hRCloseServiceHandle(self._rpcsvcctl, svc_handle)
                svc_handle = None

    def delete_service(self, name):
        if self._svcmgr is None:
            raise RuntimeError("ServiceManager must be open() first")

        svc_handle = None

        try:
            try:
                resp = impkt_scmr.hROpenServiceW(
                    self._rpcsvcctl, self._svcmgr, name + "\x00")
                svc_handle = resp["lpServiceHandle"]
            except Exception as exc:
                if str(exc).find("ERROR_SERVICE_DOES_NOT_EXIST") >= 0:
                    return
                else:
                    raise

            try:
                impkt_scmr.hRControlService(
                    self._rpcsvcctl, svc_handle,
                    impkt_scmr.SERVICE_CONTROL_STOP)
            except Exception as exc:
                if str(exc).find("ERROR_SERVICE_NOT_ACTIVE") < 0:
                    logger.info(
                        f'failed to STOP remote service "{name}": {exc}')

            # time.sleep(2.0)  # TODO: poll service status

            try:
                impkt_scmr.hRDeleteService(self._rpcsvcctl, svc_handle)
            except Exception as exc:
                if str(exc).find("ERROR_SERVICE_DOES_NOT_EXIST") < 0:
                    raise
        finally:
            if svc_handle is not None:
                with contextlib.suppress(Exception):
                    impkt_scmr.hRCloseServiceHandle(self._rpcsvcctl, svc_handle)
                svc_handle = None

    def _open_impl(self):
        if self._svcmgr is not None:
            return

        self._smbconn = self._smbconfig.spawn_smb_connection()

        self._smbtransport = impkt_transport.SMBTransport(
            remoteName=self._smbconn.getRemoteHost(),  # getRemoteName(),
            remote_host=self._smbconn.getRemoteHost(),
            dstport=self._smbconn._sess_port,
            smb_connection=self._smbconn,
            filename="\\svcctl")

        self._rpcsvcctl = self._smbtransport.get_dce_rpc()
        self._rpcsvcctl.connect()
        self._rpcsvcctl.bind(impkt_scmr.MSRPC_UUID_SCMR)

        res = impkt_scmr.hROpenSCManagerW(self._rpcsvcctl)
        self._svcmgr = res["lpScHandle"]
