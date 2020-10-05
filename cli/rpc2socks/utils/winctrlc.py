# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

"""
Support of ``CTRL_C_EVENT`` and ``CTRL_CLOSE_EVENT`` events on Windows console
using the Win32 API ``SetConsoleCtrlHandler``.

This module can be safely imported and used unconditionally on non-Windows
platforms as well, in which case its API remains unchanged only its feature is
silently disabled internally.

Only standard external modules required.
"""

import ctypes
import os
import threading
import traceback

__all__ = (
    "winctrlc_is_installed",
    "winctrlc_register_callback",
    "winctrlc_unregister_callback",
    "winctrlc_unregister_all_callbacks")

_IS_WINDOWS = os.name == "nt"
_lock = threading.Lock()
_installed = False
_user_handlers = []


def winctrlc_is_installed():
    with _lock:
        return _installed


def winctrlc_register_callback(user_handler):
    global _user_handlers

    if not callable(user_handler):
        raise ValueError("user_handler")

    with _lock:
        if not _installed:
            return False

        for hdlr in _user_handlers:
            if hdlr is user_handler:
                return True

        _user_handlers.append(user_handler)

        return True


def winctrlc_unregister_callback(user_handler):
    global _user_handlers

    with _lock:
        for idx, hdlr in enumerate(_user_handlers):
            if hdlr is user_handler:
                del _user_handlers[idx]
                return True

    return False


def winctrlc_unregister_all_callbacks():
    global _user_handlers

    with _lock:
        _user_handlers = []


if _IS_WINDOWS:
    @ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_uint)
    def _winctrl_handler(dwCtrlType):
        if dwCtrlType in (0, 2):  # (CTRL_C_EVENT, CTRL_CLOSE_EVENT)
            with _lock:
                handlers = _user_handlers[:]

            for hdlr in reversed(handlers):
                try:
                    if hdlr():
                        break
                except Exception:
                    traceback.print_exc()

            return True

        return False


def _install_handler():
    global _installed

    with _lock:
        if _IS_WINDOWS and not _installed:
            _scch = ctypes.windll.kernel32.SetConsoleCtrlHandler
            _scch.restype = ctypes.c_long
            _scch.argtypes = [ctypes.c_void_p, ctypes.c_long]

            _scch(_winctrl_handler, 1)

            _installed = True


_install_handler()
