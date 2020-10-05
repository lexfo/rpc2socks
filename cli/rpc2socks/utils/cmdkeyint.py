# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

"""
A wrapper around `cmd` to handle `KeyboardInterrupt`, and ``Ctrl-C`` event if on
Windows.
"""

import cmd
import contextlib

try:
    from . import winctrlc
except ImportError:
    winctrlc = None


__all__ = ("Cmd", )


class Cmd(cmd.Cmd):
    def __init__(self, *args, **kwargs):
        if winctrlc is not None:
            winctrlc.winctrlc_register_callback(self._on_cmd_winctrlc)

        super().__init__(*args, **kwargs)

    def __del__(self):
        with contextlib.suppress(Exception):
            if winctrlc is not None:
                winctrlc.winctrlc_unregister_callback(self._on_cmd_winctrlc)

    def do_KeyboardInterrupt(self, argsline):
        return True

    def do_EOF(self, argsline):
        if "KeyboardInterrupt" in self.cmdqueue:
            return self.do_KeyboardInterrupt("")
        else:
            return None

    def cmdloop(self, intro=None):
        while True:
            try:
                super().cmdloop(intro=intro)
                break
            except KeyboardInterrupt:
                self.cmdqueue.insert(0, "KeyboardInterrupt")

    def _on_cmd_winctrlc(self):
        # CAUTION: we are not in cmdloop() context here so we must find a way to
        # interrupt its call to input() remotely. This is hacky but allows at
        # least graceful shutdown
        self.cmdqueue.insert(0, "KeyboardInterrupt")
        self.stdin.close()
