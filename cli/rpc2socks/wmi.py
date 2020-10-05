# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import time

from impacket.dcerpc.v5 import dtypes as impkt_dtypes
from impacket.dcerpc.v5.dcom import wmi as impkt_wmi

from . import smb

from .utils import dispatcher
from .utils import logging

__all__ = ("wmi_rexec", )

logger = logging.get_internal_logger(__name__)


def wmi_rexec(smb_config, cmdline, *,
              workdir="C:\\", wait=False, wait_poll_delay=2.0):
    assert isinstance(smb_config, smb.SmbConfig)

    if wait is True:
        pass
    elif wait is False:
        wait = 0
    elif wait is None:
        wait = True
    elif isinstance(wait, (int, float)):
        if wait < 0:
            raise ValueError("wait delay < 0")
    else:
        raise ValueError("wait")

    dcom_conn = None
    iWbemServices = None
    win32Process = None

    try:
        dcom_conn = smb_config.spawn_dcom_connection()

        iInterface = dcom_conn.CoCreateInstanceEx(
            impkt_wmi.CLSID_WbemLevel1Login,
            impkt_wmi.IID_IWbemLevel1Login)

        iWbemLevel1Login = impkt_wmi.IWbemLevel1Login(iInterface)

        iWbemServices = iWbemLevel1Login.NTLMLogin(
            "//./root/cimv2", impkt_dtypes.NULL, impkt_dtypes.NULL)

        iWbemLevel1Login.RemRelease()
        iWbemLevel1Login = None

        win32Process, _ = iWbemServices.GetObject("Win32_Process")

        create_result = win32Process.Create(cmdline, workdir, None)
        child_pid = create_result.getProperties()["ProcessId"]["value"]

        logger.info(f"created remote process {child_pid}")
        logger.debug(f"cmdline: {cmdline}")

        if wait:
            logger.debug(f"waiting for remote process {child_pid} to terminate")

            wait_start = time.monotonic()

            while True:
                iEnumWbemClassObject = iWbemServices.ExecQuery(
                    f"SELECT * FROM Win32_Process WHERE handle = {child_pid}")

                try:
                    child = iEnumWbemClassObject.Next(0xffffffff, 1)[0]
                except impkt_wmi.DCERPCSessionError as exc:
                    if exc.error_code == 1:  # == WBEM_S_FALSE
                        return True  # wait successful
                    else:
                        logger.exception(
                            f"failed to wait for remote process {child_pid}")
                        return False  # wait failed

                # TEST
                # child_props = child.getProperties()
                # from pprint import pprint
                # pprint(list(child_props.keys()))
                # pprint(child_props)
                # TESTEND

                if wait is not True:
                    elapsed = time.monotonic() - wait_start
                    if elapsed >= wait:
                        return False

                time.sleep(wait_poll_delay)

        return True  # execution successful
    except Exception as exc:
        if logger.isEnabledFor(logging.DEBUG):
            logger.exception(f"failed to execute remote command: {cmdline}")
        else:
            logger.error(f"failed to execute remote command: {cmdline}")
            logger.error(f"error was: {exc}")
            logger.error("enable full verbose mode for more info")
        return None  # an error occurred
    finally:
        win32Process = None

        if iWbemServices is not None:
            with contextlib.suppress(Exception):
                iWbemServices.disconnect()
            iWbemServices = None

        if dcom_conn is not None:
            with contextlib.suppress(Exception):
                dcom_conn.disconnect()
            dcom_conn = None

    return None  # an error occurred
