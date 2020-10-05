# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

from .vendor import loggex


PACKAGE_NAME = __name__.split(".", maxsplit=1)[0]

# standard log levels
CRITICAL = loggex.CRITICAL
FATAL = loggex.FATAL
ERROR = loggex.ERROR
WARNING = loggex.WARNING
WARN = loggex.WARN
INFO = loggex.INFO
DEBUG = loggex.DEBUG
NOTSET = loggex.NOTSET

# extra custom log levels
PPRINT = loggex.PPRINT
HINFO = loggex.HINFO
ASSERTION = loggex.ASSERTION


def get_internal_logger(name=None):
    """
    Like `logging.getLogger` but returned logger's *name* is prefixed with
    ``<THIS_PACKAGE_NAME>.``

    If *name* is false or empty, the returned logger is package's default
    logger.
    """
    if not name:
        name = PACKAGE_NAME
    else:
        name = name.rsplit(".", maxsplit=1)[-1]
        name = PACKAGE_NAME + "." + name

    return loggex.getLogger(name)


def set_root_log_level(new_level):
    """Set root logger's level"""
    return loggex.getLogger(None).setLevel(new_level)


def _bootstrap():
    """
    Create and initialize both the so-called "root" logger as well as this
    package's own default logger with per-level formatting and coloring
    """
    # configure per-level colored logging
    loggex.install()

    # module-level log functions
    # default_logger = get_internal_logger()
    # globvars = globals()
    # for sym in (
    #         "critical", "fatal", "error", "exception", "warning", "info",
    #         "debug", "log",
    #         "pprint", "hinfo", "assertion"):
    #     globvars[sym] = getattr(default_logger, sym)

    # extra feature: some modules are known to be extensively verbose
    # loggex.getLogger("asyncio").setLevel(loggex.INFO)
    # loggex.getLogger("chardet.charsetprober").setLevel(loggex.INFO)


_bootstrap()

# shorthands to standard log functions; initialized by _bootstrap()
# critical = None
# fatal = None
# error = None
# exception = None
# warning = None
# info = None
# debug = None
# log = None

# shorthands to loggex-specific log functions; initialized by _bootstrap()
# pprint = None
# hinfo = None
# assertion = None
