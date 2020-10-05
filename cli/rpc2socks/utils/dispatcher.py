# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

"""
A basic and thread-safe implementation of the Dispatcher-Observer pattern where
`Observer` are stored as `weakref` by the `Dispatcher`.
"""

import threading
import weakref

from . import _utils

__all__ = ("Dispatcher", "Observer")


class Observer:
    def __init__(self):
        super().__init__()

    def _observer_event_launch_pad(self, dispatcher, event_name, event_method,
                                   *args, **kwargs):
        """
        Event dispatching is implemented at observer's level so that client
        implementor has a chance if desired to implement a **single point** of
        event dispatching (by overriding this method) before the reciptient
        method of an event gets called.
        """
        if event_method is not None:
            try:
                event_method(*args, **kwargs)
            except Exception:
                if dispatcher.dispatcher_raise_errors:
                    raise
                else:
                    # keep local ref so that we do not need to lock its access
                    logger = dispatcher.dispatcher_logger
                    if logger is not None:
                        logger.exception(
                            f"{_utils.get_fullname(self)}.{event_name} raised "
                            f"an exception")


class Dispatcher:
    def __init__(self, *, dispatcher_raise_errors=False, dispatcher_logger=None,
                 observers=()):
        super().__init__()

        self._dispatcher_lock = threading.Lock()
        self._dispatcher_observers = []
        self._dispatcher_raise_errors = dispatcher_raise_errors
        self._dispatcher_logger = dispatcher_logger

        if isinstance(observers, (tuple, list)):
            for obs in observers:
                self.register_observer(obs)
        elif observers is not None:
            # assume *observers* is a single observer object
            self.register_observer(observers)

    @property
    def dispatcher_raise_errors(self):
        return self._dispatcher_raise_errors

    @property
    def dispatcher_logger(self):
        return self._dispatcher_logger

    def set_dispatcher_raise_errors(self, enable):
        with self._dispatcher_lock:
            self._dispatcher_raise_errors = bool(enable)

    def set_dispatcher_logger(self, logger):
        with self._dispatcher_lock:
            self._dispatcher_logger = logger  # can be None

    def register_observer(self, observer):
        with self._dispatcher_lock:
            for obsw in self._dispatcher_observers:
                obs = obsw()
                if obs is observer:
                    break
            else:
                self._dispatcher_observers.append(weakref.ref(observer))

    def unregister_observer(self, observer):
        with self._dispatcher_lock:
            for idx, obsw in enumerate(self._dispatcher_observers):
                obs = obsw()
                if obs is observer:
                    del self._dispatcher_observers[idx]
                    return True

        return False

    def unregister_all_observers(self):
        with self._dispatcher_lock:
            self._dispatcher_observers = []

    def notify_observers(self, event_name, *args, **kwargs):
        with self._dispatcher_lock:
            if not self._dispatcher_observers:
                return

            observers = self._dispatcher_observers[:]

        cleanup_required = False

        for obsw in observers:
            obs = obsw()
            if obs is None:
                cleanup_required = True
                continue

            try:
                dispatch_method = getattr(obs, "_observer_event_launch_pad")
            except AttributeError:
                raise NotImplementedError(
                    f"{_utils.get_fullname(obs)}._observer_event_launch_pad() "
                    f"not implemented; object not derived from Observer?")

            try:
                event_method = getattr(obs, event_name)
            except AttributeError:
                # if self._dispatcher_logger is not None:
                #     self._dispatcher_logger.warning(
                #         f"{_utils.get_fullname(self)}.{event_name}() not "
                #         f"implemented")

                event_method = None

            dispatch_method(self, event_name, event_method, *args, **kwargs)

        if cleanup_required:
            with self._dispatcher_lock:
                observers = []

                for obsw in self._dispatcher_observers:
                    if obsw() is not None:
                        observers.append(obsw)

                self._dispatcher_observers = observers
