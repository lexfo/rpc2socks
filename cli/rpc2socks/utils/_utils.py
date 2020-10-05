# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import datetime
import os
import sys

__all__ = (
    "UNSET",
    "NoDict",
    "ask", "get_fullname", "get_fullnames", "humanize_elapsed_seconds",
    "reconfigure_output_streams")


#: Used to allow `None` to be a regular value in some cases
UNSET = object()


class NoDict(object):
    """Subclass `object` to deny the creation of a ``__dict__``"""
    __slots__ = ()


def ask(question, *, choices="Yn", ofile=sys.stderr):
    if not choices or not isinstance(choices, str):
        raise ValueError("choices")

    default = ""
    for c in choices:
        if c.lower() == c.upper():
            raise ValueError(f'not a valid choice character "{c}"')
        elif c == c.upper():
            if default:
                raise ValueError("multiple default choices in: " + choices)
            else:
                default = c.lower()

    question = question.strip().rstrip("?").rstrip()
    question += "? [" + "/".join(choices) + "] "

    choices = choices.lower()

    while True:
        ofile.write(question)
        ofile.flush()
        ans = input()
        # ofile.write("\n")
        # ofile.flush()

        ans = ans.strip()

        if not ans:
            if default:
                return default
            continue
        else:
            ans = ans[0].lower()

            if ans not in choices:
                ofile.write("unknown input, please retry\n")
                continue

            return ans


def get_fullname(something):
    """Get the full Python name of *something*"""
    something_orig = something

    if not isinstance(something, type):
        try:
            something = something.__class__
        except AttributeError:
            something = UNSET

    if something != UNSET and isinstance(something, type):
        modname = something.__module__

        if (not modname or
                modname == "__main__" or
                modname == str.__class__.__module__):  # __builtin__
            fullname = something.__name__
        else:
            fullname = modname + "." + something.__name__

        if fullname:
            return fullname

    raise ValueError(
        f"failed to get the full Python name of {type(something_orig)}")


def get_fullnames(somethings):
    return [get_fullname(klass) for klass in somethings]


def humanize_elapsed_seconds(seconds):
    """
    Convert a value in *seconds* (`int` or `float`) and return a `str` of the
    form ``0:00:00.123`` or ``0:00:00``
    """
    elapsed = str(datetime.timedelta(seconds=seconds))

    # "0:00:00.123000" to "0:00:00.123"
    # caution: when elapsed time is zero, str(timedelta) returns "0:00:00",
    # hence the test
    if "." in elapsed:
        elapsed = elapsed.rstrip("0")
        if elapsed[-1] == ".":  # in case result was "x:xx:xx.000"
            elapsed += "000"

    return elapsed


def reconfigure_output_streams():
    """
    On Windows with Python 3.7+ only, reconfigure `sys.stdout` and `sys.stderr`
    to avoid encoding errors.

    Does nothing in any other environment.
    """
    if os.name == "nt" and sys.version_info >= (3, 7):
        for stream in (sys.stdout, sys.stderr):
            if stream.isatty():
                stream.reconfigure(errors="replace")
            else:
                stream.reconfigure(encoding="utf-8", errors="strict")
