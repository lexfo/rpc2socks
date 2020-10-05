"""
A wrapper around the standard `logging` package that can be used as a drop-in
replacement and that enables per-level formatting and coloring as well as extra
log levels.

Python 3.8 not compulsory but recommended due to the use of the ``stacklevel``
parameter in the ``pprint`` and ``assertion`` extra methods this module
implements (search for ``_PYTHON38`` in the source code of this module).

This module should be imported as soon as possible, and right before calling
the ``install`` function of this module.

**Optional** Color and styling features offered via the
`colorama <https://github.com/tartley/colorama>`_ third-party package.

This module tries to import the *colorama* package as follows (in chronological
order):

    * ``from . import colorama``
    * upon failure of the above: ``import colorama``
    * upon failure of the above, this module does not raise an exception but
      color and styling features are disabled

For the sake of maintainability, efforts have been made to follow the same
layout in the source code of this module than standard `logging` module (i.e.
``logging.__init__.py``).
"""

import collections.abc as _mod_collections_abc
import copy as _mod_copy
import functools as _mod_functools
import logging as _mod_logging
import pprint as _mod_pprint
import sys as _mod_sys
import threading as _mod_threading
import types as _mod_types

try:
    from . import colorama as _mod_colorama
except ImportError:
    try:
        import colorama as _mod_colorama
    except ImportError:
        _mod_colorama = None


__all__ = ()


_PYTHON38 = _mod_sys.version_info >= (3, 8)
_INSTALLED = False


def install():
    global _INSTALLED

    if (not hasattr(_mod_logging, "root") or
            not hasattr(_mod_logging.root, "handlers") or
            not hasattr(_mod_logging.Logger, "manager") or
            not hasattr(_mod_logging, "_lock") or
            not hasattr(_mod_logging, "_defaultFormatter")):
        raise RuntimeError(
            "internals of the standard logging package changed, this module "
            "must be updated accordingly, please contact the developers")

    addLevelName(PPRINT, "PPRINT")
    addLevelName(HINFO, "HINFO")
    addLevelName(ASSERTION, "ASSERTION")

    setLogRecordFactory(LogRecord)
    setLoggerClass(Logger)

    # gentle but intrusive patching...

    if _mod_logging.Logger.manager.logRecordFactory is not None:
        _mod_logging.Logger.manager.setLogRecordFactory(LogRecord)

    if _mod_logging.Logger.manager.loggerClass is not None:
        _mod_logging.Logger.manager.setLoggerClass(Logger)

    _mod_logging._defaultFormatter = _defaultFormatter
    _mod_logging.lastResort = lastResort

    # patch root logger if needed
    _mod_logging._acquireLock()
    try:
        if not hasattr(_mod_logging.root, "hinfo"):
            patch_with_extra_methods(_mod_logging.root)

        globvars = globals()
        for method_name in ("pprint", "hinfo", "assertion"):
            globvars[method_name] = \
                getattr(_mod_logging.root, method_name)
    finally:
        _mod_logging._releaseLock()
        _INSTALLED = True

    # this force re-creates handlers by default
    basicConfig(force=True)


def patch_with_extra_methods(something):
    """
    Patch a `logging.Logger` or a `logging.LoggerAdapter` derived **object** or
    **type**.
    """
    if not hasattr(something, "log"):
        raise ValueError("something")

    if isinstance(something, type):
        setattr(something, "pprint", _pprint_method)
        setattr(something, "hinfo", _mod_functools.partialmethod(something.log, HINFO))
        setattr(something, "assertion", _assertion_method)
    else:
        setattr(something, "pprint", _mod_types.MethodType(_pprint_method, something))
        setattr(something, "hinfo", _mod_functools.partial(something.log, HINFO))
        setattr(something, "assertion", _mod_types.MethodType(_assertion_method, something))


#-------------------------------------------------------------------------------
# Levels
#-------------------------------------------------------------------------------
# standard log levels
CRITICAL = _mod_logging.CRITICAL
FATAL = _mod_logging.FATAL
ERROR = _mod_logging.ERROR
WARNING = _mod_logging.WARNING
WARN = _mod_logging.WARN
INFO = _mod_logging.INFO
DEBUG = _mod_logging.DEBUG
NOTSET = _mod_logging.NOTSET

# extra custom log levels
# CAUTION: any change must be reported to the install()
PPRINT = DEBUG + 1
HINFO = INFO + 1  # "highlighted info header"
ASSERTION = ERROR + 1


getLevelName = _mod_logging.getLevelName
addLevelName = _mod_logging.addLevelName

currentframe = _mod_logging.currentframe


#-------------------------------------------------------------------------------
# LogRecord
#-------------------------------------------------------------------------------
class LogRecord(_mod_logging.LogRecord):
    ANSI_CODES = {}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.__dict__.update(self.ANSI_CODES)


# ppopulates LogRecord.ANSI_CODES
if _mod_colorama is not None:
    for prefix, colors in (
            ("FORE_", _mod_colorama.Fore),
            ("BACK_", _mod_colorama.Back),
            ("STYLE_", _mod_colorama.Style)):
        for name in dir(colors):
            if name and name[0] != "_":
                LogRecord.ANSI_CODES[prefix + name] = getattr(colors, name)


setLogRecordFactory = _mod_logging.setLogRecordFactory
getLogRecordFactory = _mod_logging.getLogRecordFactory
makeLogRecord = _mod_logging.makeLogRecord


#-------------------------------------------------------------------------------
# Formatter
#-------------------------------------------------------------------------------
# non-standard: default date format
DATEFMT_TIMEONLY = "%H:%M:%S"
DATEFMT_DATETIME = "%Y-%m-%d %H:%M:%S"
DEFAULT_DATEFMT = DATEFMT_TIMEONLY

# non-standard: default formatting for colored output ("%" style)
if _mod_colorama is None:
    DEFAULT_FORMAT_COLORED = {NOTSET: "%(levelname)s:%(name)s:%(message)s"}
else:
    DEFAULT_FORMAT_COLORED = {
        DEBUG:     "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_MAGENTA)s "      "%(levelname)s: %(message)s %(FORE_LIGHTMAGENTA_EX)s<%(module)s:%(lineno)d>",
        PPRINT:    "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_MAGENTA)s "      "%(levelname)s<%(module)s:%(lineno)d>: %(message)s",
        INFO:      "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_WHITE)s "        "%(message)s",
        HINFO:     "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_LIGHTCYAN_EX)s " "%(message)s",
        WARNING:   "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_YELLOW)s "       "%(levelname)s: %(message)s",
        ERROR:     "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_RED)s "          "%(levelname)s: %(message)s",
        ASSERTION: "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_RED)s "          "%(levelname)s: %(message)s %(FORE_LIGHTMAGENTA_EX)s<%(module)s:%(lineno)d>",
        CRITICAL:  "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_RED)s "          "%(levelname)s: %(message)s",
        NOTSET:    "%(FORE_LIGHTWHITE_EX)s%(asctime)s.%(msecs)03d%(FORE_WHITE)s "        "%(levelname)s: %(message)s"}

# non-standard: default formatting for non-colored output ("%" style)
if _mod_colorama is None:
    DEFAULT_FORMAT_NONCOLORED = {NOTSET: "%(levelname)s:%(name)s:%(message)s"}
else:
    DEFAULT_FORMAT_NONCOLORED = {
        DEBUG:     "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s    %(levelname)s: %(message)s <%(module)s:%(lineno)d>",
        PPRINT:    "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s    %(levelname)s <%(module)s:%(lineno)d>: %(message)s",
        INFO:      "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s .. %(message)s",
        HINFO:     "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s -- %(message)s",
        WARNING:   "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s !! %(levelname)s: %(message)s",
        ERROR:     "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s ** %(levelname)s: %(message)s",
        ASSERTION: "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s    %(levelname)s: %(message)s <%(module)s:%(lineno)d>",
        CRITICAL:  "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s ** %(levelname)s: %(message)s",
        NOTSET:    "%(asctime)s.%(msecs)03d [%(processName)s:%(process)d] %(name)s ?? %(levelname)s: %(message)s"}


# non-standard: our drop-in replacement to logging.Formatter
class PerLevelFormatter(_mod_logging.Formatter):
    """
    A `logging.Formatter` derived class that **optionally** allows per-level
    formatting and coloring.

    Constructor's arguments are the same than `logging.Formatter` but defaults
    values for *fmt* and *datefmt* fit better to the purpose of this module.
    """

    def __init__(self, fmt=None, datefmt=None, style="%", *args, **kwargs):
        # validate *style*
        if style not in _STYLES:
            raise ValueError("style")

        # validate *fmt*
        if not fmt:
            fmt = _STYLES[style][1]
        elif isinstance(fmt, str):
            fmt = {NOTSET: fmt}
        elif isinstance(fmt, _mod_collections_abc.Mapping):
            try:
                fmt = fmt.copy()
            except AttributeError:
                fmt = _mod_copy.copy(fmt)
        else:
            raise ValueError("fmt")

        # ensure *fmt* got a fallback format value (i.e. NOTSET)
        for lvl in (NOTSET, INFO, HINFO):
            if lvl in fmt:
                if lvl != NOTSET:
                    fmt[NOTSET] = fmt[lvl]
                break
        else:
            raise ValueError("fmt")

        # apply our own default *datefmt*
        if not datefmt:
            datefmt = DEFAULT_DATEFMT

        self._perlevel_format = fmt

        super().__init__(
            fmt=fmt[NOTSET], datefmt=datefmt, style=style, *args, **kwargs)

    def get_level_format(self, levelno):
        try:
            return self._perlevel_format[levelno]
        except KeyError:
            return self._perlevel_format[NOTSET]

    def format(self, record):
        # save some logging.Formatter properties
        old_fmt = self._fmt
        old_style_fmt = self._style._fmt

        try:
            # overwrite logging.Formatter context temporarily
            self._fmt = self.get_level_format(record.levelno)
            self._style._fmt = self._fmt

            # do format
            result_message = super().format(record)
        finally:
            # restore logging.Formatter context
            self._fmt = old_fmt
            self._style._fmt = old_style_fmt

        # append the RESET_ALL sequence only if record.color is found in the
        # resulting message
        if "\033" in result_message and not result_message.endswith(
                _mod_colorama.Style.RESET_ALL):
            result_message += _mod_colorama.Style.RESET_ALL

        return result_message


PercentStyle = _mod_logging.PercentStyle
StrFormatStyle = _mod_logging.StrFormatStyle
StringTemplateStyle = _mod_logging.StringTemplateStyle

BASIC_FORMAT = DEFAULT_FORMAT_COLORED

_STYLES = {
    "%": (PercentStyle, BASIC_FORMAT),
    "{": (StrFormatStyle, "{levelname}:{name}:{message}"),
    "$": (StringTemplateStyle, "${levelname}:${name}:${message}")}

Formatter = _mod_logging.Formatter
BufferingFormatter = _mod_logging.BufferingFormatter

_defaultFormatter = PerLevelFormatter()


#-------------------------------------------------------------------------------
# Filter
#-------------------------------------------------------------------------------
Filter = _mod_logging.Filter
Filterer = _mod_logging.Filterer


#-------------------------------------------------------------------------------
# Handler
#-------------------------------------------------------------------------------
Handler = _mod_logging.Handler


class StreamHandler(_mod_logging.StreamHandler):
    """
    A `logging.StreamHandler` class that can automatically and optionally strip
    ANSI sequences from the data to write in case target *stream* is not a TTY
    """

    def __init__(self, stream=None, *, strip_colors=None, **kwargs):
        """
        Constructor.

        If *stream* is specified, the instance will use it for logging output;
        otherwise, *sys.stderr* will be used.

        *strip_colors* can be `None` or must evaluate to a boolean. `None` means
        stripping will be applied automatically if *stream* is **not** a TTY.
        A boolean value force stripping behavior regardless of the type of
        *stream*.
        """
        # sys.stderr is the default, as per standard logging.StreamHandler
        # implementation
        if stream is None:
            stream = _mod_sys.stderr

        # normalize strip_colors value for colorama
        if strip_colors is None:
            pass
        elif not strip_colors:
            strip_colors = False
        else:
            strip_colors = True

        # setup colorama stream wrapper
        if _mod_colorama is not None:
            stream = _mod_colorama.initialise.wrap_stream(
                stream, convert=None, strip=strip_colors, autoreset=False,
                wrap=True)

        super().__init__(stream=stream, **kwargs)


FileHandler = _mod_logging.FileHandler


# `logging.lastResort`
# this honors `logging`'s defaults
lastResort = StreamHandler(stream=_mod_sys.stderr)
lastResort.setLevel(WARNING)


#-------------------------------------------------------------------------------
# Manager
#-------------------------------------------------------------------------------
# PlaceHolder = _mod_logging.PlaceHolder  # intended for internal use only


def setLoggerClass(klass):
    if not issubclass(klass, Logger):
        raise TypeError(
            "logger not derived from loggex.Logger: " + klass.__name__)

    return _mod_logging.setLoggerClass(klass)


getLoggerClass = _mod_logging.getLoggerClass

Manager = _mod_logging.Manager


#-------------------------------------------------------------------------------
# Logger
#-------------------------------------------------------------------------------

# CAUTION: this class is setup later in this module with function
# patch_with_extra_methods()
class Logger(getLoggerClass()):
    pass


# CAUTION: this class is setup later in this module with function
# patch_with_extra_methods()
class LoggerAdapter(_mod_logging.LoggerAdapter):
    pass


#-------------------------------------------------------------------------------
# Configuration
#-------------------------------------------------------------------------------
stdBasicConfig = _mod_logging.basicConfig


def basicConfig(**kwargs):
    """
    A `logging.basicConfig` wrapper with default values that fit better to the
    purpose of this module.

    Call ``stdBasicConfig`` for the standard unwrapped `logging.basicConfig`.
    """
    def _stdBasicConfigWrapper(**kwargs):
        # We cannot just blindly rely on the implementation of the standard
        # basicConfig() because it instantiates a standard Formatter class
        # instead of relying on its module-level *_defaultFormatter* value,
        # which we patched in install() to use our own PerLevelFormatter class.
        #
        # Same goes for FileHandler and StreamHandler classes.
        #
        # This is not a desired behavior in our case, hence the temporary
        # monkey-patching below, which helps avoiding having to re-implement
        # basicConfig() ourselves.

        old_formatter = _mod_logging.Formatter
        old_filehandler = _mod_logging.FileHandler
        old_streamhandler = _mod_logging.StreamHandler
        old_basicformat = _mod_logging.BASIC_FORMAT
        old_styles = _mod_logging._STYLES

        _mod_logging._acquireLock()
        try:
            _mod_logging.Formatter = PerLevelFormatter
            _mod_logging.FileHandler = FileHandler
            _mod_logging.StreamHandler = StreamHandler
            _mod_logging.BASIC_FORMAT = BASIC_FORMAT
            _mod_logging._STYLES = _STYLES

            # backward compatibility for *force* arg (3.8+)
            if not _PYTHON38:
                if kwargs.pop("force", False):
                    for hdlr in _mod_logging.root.handlers[:]:
                        _mod_logging.root.removeHandler(hdlr)
                        hdlr.close()

            # CAUTION: do not call "_mod_logging.basicConfig" here because we
            # may have monkey-patched it already :)
            res = stdBasicConfig(**kwargs)
        finally:
            _mod_logging.Formatter = old_formatter
            _mod_logging.FileHandler = old_filehandler
            _mod_logging.StreamHandler = old_streamhandler
            _mod_logging.BASIC_FORMAT = old_basicformat
            _mod_logging._STYLES = old_styles

            _mod_logging._releaseLock()

        return res

    if not _INSTALLED:
        raise RuntimeError("install() not called first")

    # kwargs.setdefault("force", True)

    return _stdBasicConfigWrapper(**kwargs)


#-------------------------------------------------------------------------------
# Utility functions
#-------------------------------------------------------------------------------
getLogger = _mod_logging.getLogger

critical = _mod_logging.critical
fatal = _mod_logging.fatal
error = _mod_logging.error
exception = _mod_logging.exception
warning = _mod_logging.warning
# warn = _mod_logging.warn  # deprecated; commented to prevent bad practice
info = _mod_logging.info
debug = _mod_logging.debug
log = _mod_logging.log
disable = _mod_logging.disable
shutdown = _mod_logging.shutdown

NullHandler = _mod_logging.NullHandler

captureWarnings = _mod_logging.captureWarnings


#-------------------------------------------------------------------------------
# LoggEx functions
#-------------------------------------------------------------------------------
# to be initialized by install()
pprint = None
hinfo = None
assertion = None


def _pprint_method(self, obj, **kwargs):
    if self.isEnabledFor(PPRINT):
        msg = _mod_pprint.pformat(obj)

        if _PYTHON38:
            stacklevel = kwargs.pop("stacklevel", 1)
            stacklevel += 1  # skip this very function call
            kwargs["stacklevel"] = stacklevel
        else:
            # TODO / FIXME: without the support of the stacklevel arg, the wrong
            # info will be logged
            pass

        self.log(PPRINT, msg, **kwargs)


def _assertion_method(self, condition_result, *args, **kwargs):
    if self.isEnabledFor(ASSERTION) and not condition_result:
        if not args:
            args = ("Assertion failed", )

        if _PYTHON38:
            stacklevel = kwargs.pop("stacklevel", 1)
            stacklevel += 1  # skip this very function call
            kwargs["stacklevel"] = stacklevel
        else:
            # TODO / FIXME: without the support of the stacklevel arg, the wrong
            # info will be logged
            pass

        self.log(ASSERTION, *args, **kwargs)


patch_with_extra_methods(Logger)
patch_with_extra_methods(LoggerAdapter)
