# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import concurrent.futures
import functools
import os
import threading
import weakref

from . import _utils
from . import logging


__all__ = ("ThreadPool", "MethodCallTask", "FunctionCallTask", "PoolTaskBase")


logger = logging.get_internal_logger(__name__)


class PoolTaskBase(_utils.NoDict):
    __slots__ = ("pool_ref", "label", "future")

    def __init__(self, pool):
        super().__init__()

        self.pool_ref = weakref.ref(pool)
        self.label = self.__class__.__name__
        self.future = None

    @property
    def pool(self):
        return self.pool_ref()  # may be None

    def get_callee(self):
        raise NotImplementedError

    def submit(self, executor):
        if not isinstance(executor, concurrent.futures.Executor):
            raise ValueError("executor")

        if self.future is not None:
            raise RuntimeError(f"task {self.label} already submitted")

        try:
            self.future = executor.submit(self._on_run)
        except RuntimeError:
            # may occur if executor.shutdown() was called
            return None

        self.future.add_done_callback(self._on_done)

        return self.future

    def _on_run(self):
        callee = self.get_callee()
        if callee is not None:
            try:
                callee()
            except Exception:
                logger.exception(
                    f"unhandled exception occurred while running {self.label}")

    def _on_done(self, future):
        # notify the parent pool object
        pool = self.pool
        if pool is not None:
            pool._on_future_done(future)


class MethodCallTask(PoolTaskBase):
    __slots__ = ("method_name", "method_ref", "args", "kwargs")

    def __init__(self, pool, obj, method_name, *args, **kwargs):
        super().__init__(pool)

        try:
            method = getattr(obj, method_name)
        except AttributeError:
            method = None

        if method is None or not callable(method):
            raise ValueError(
                f"{_utils.get_fullname(obj)}.{method_name} does not designate "
                f"a callable")

        self.method_name = method_name
        self.method_ref = weakref.WeakMethod(method)
        self.args = args
        self.kwargs = kwargs

        # update task's label
        self.label = "{}.{}".format(
            _utils.get_fullname(obj), method_name)

    def get_callee(self):
        method = self.method_ref()
        if method is None:
            return None

        return functools.partial(method, *self.args, **self.kwargs)


class FunctionCallTask(PoolTaskBase):
    __slots__ = ("func_ref", "args", "kwargs")

    def __init__(self, pool, func, *args, **kwargs):
        super().__init__(pool)

        if not callable(func):
            raise ValueError(f"{_utils.get_fullname(func)} not a callable")

        self.func_ref = weakref.ref(func)
        self.args = args
        self.kwargs = kwargs

        # update task's label
        self.label = "<callable:{}>".format(_utils.get_fullname(func))

    def get_callee(self):
        func = self.func_ref()
        if func is None:
            return None

        return functools.partial(func, *self.args, **self.kwargs)


class _DoneTasksCallbackTask(FunctionCallTask):
    __slots__ = ()

    def __init__(self, pool, callback, done_tasks):
        super().__init__(pool, callback, done_tasks)

        # update task's label
        self.label = "<done_tasks_callback:{}>".format(
            _utils.get_fullname(callback))

    @property
    def done_tasks(self):
        return self.args[0]


class ThreadPool:
    """
    A generic, non-throttleable thread pool to launch concurrent tasks in a FIFO
    fashion
    """

    def __init__(self, *, max_workers=None, tasks_done_callback=None):
        if max_workers is None or max_workers == 0:
            max_workers = (os.cpu_count() or 1) * 3
            max_workers = min(32, max_workers)
        elif not isinstance(max_workers, int) or max_workers < 0:
            raise ValueError("max_workers")

        if tasks_done_callback is not None and not callable(tasks_done_callback):
            raise ValueError("tasks_done_callback")

        # config
        self._max_workers = max_workers
        self._tasks_done_callback = tasks_done_callback

        # low-level state
        self._lock = threading.RLock()
        self._quit = False
        self._quit_on_idle = False
        self._event = threading.Event()

        # pools of objects
        self._tasks_inbox = []
        self._tasks_running = {}  # collections.OrderedDict()  # {future: task}
        self._tasks_done = []

        # maintenance thread
        self._thread = threading.Thread(
            target=self._thread_main,
            name=self.__class__.__name__,
            daemon=True)

        # tasks executor
        self._executor = None

    @property
    def tasks_done_callback(self):
        with self._lock:
            return self._tasks_done_callback

    @tasks_done_callback.setter
    def tasks_done_callback(self, callback):
        if callback is not None and not callback(callback):
            raise ValueError("callback")

        with self._lock:
            self._tasks_done_callback = callback

    def start(self):
        """Start the maintenance thread"""
        with self._lock:
            if not self._thread.is_alive():
                # reset state first
                self._quit = False
                self._quit_on_idle = False

                if self._executor is not None:
                    self._executor.shutdown(wait=False)
                    self._executor = None

                logger.debug(
                    f"launching session thread pool with {self._max_workers} "
                    f"workers max")
                self._executor = concurrent.futures.ThreadPoolExecutor(
                    max_workers=self._max_workers,
                    thread_name_prefix=self.__class__.__name__)

                # do not clear the event flag here. something may have come up
                # while the maintenance thread was not launched yet.
                # self._event.clear()

                self._thread.start()

    def is_alive(self):
        """Is the maintenance thread alive?"""
        with self._lock:
            return self._thread.is_alive()

    def request_termination(self):
        """
        Request the maintenance thread to terminate gracefully even if tasks are
        still pending.

        A call to this method is usually followed by a call to `join`.

        .. seealso:: `request_termination_on_idle`, `join`
        """
        with self._lock:
            if self._thread.is_alive():
                self._quit = True
                self._executor.shutdown(wait=False)
                self._event.set()

    def request_termination_on_idle(self):
        """
        Request the maintenance thread to terminate only once all the tasks are
        done.

        A call to this method is usually followed by a call to `join`.

        .. seealso:: `request_termination`, `join`
        """
        with self._lock:
            if self._thread.is_alive():
                self._quit_on_idle = True
                self._event.set()

    def join(self):
        """
        Wait for the maintenance thread to terminate.

        Either `request_termination_on_idle` or `request_termination` must have
        been called before.

        Return `True` if the join was successful, `False` otherwise.

        .. seealso:: `request_termination`, `request_termination_on_idle`
        """
        # TODO: support a timeout parameter?
        # * this would have to be done manually - in a loop - since we have to
        #   wait for both self._thread and self._executor
        # * or "self._executor.shutdown(wait=True)" could be called from the
        #   maintenance thread but then we would have no control over it still

        with self._lock:
            alive = self._thread.is_alive()
            must_quit = self._quit
            quit_requested = self._quit or self._quit_on_idle

        if alive:
            if not quit_requested:
                raise RuntimeError(
                    "trying to join() without having called "
                    "request_termination() or request_termination_on_idle()")

            if must_quit:
                if self._executor is not None:
                    self._executor.shutdown(wait=True)

            self._thread.join()

            if not must_quit and self._executor is not None:
                self._executor.shutdown(wait=True)

        elif self._executor is not None:
            self._executor.shutdown(wait=True)

        with self._lock:
            if not self._thread.is_alive():
                # reset state
                self._quit = False
                self._quit_on_idle = False
                self._event.clear()
                self._executor = None

                return True  # joined successfully

        return False

    def push_method_call(self, obj, method_name, *args, **kwargs):
        task = MethodCallTask(self, obj, method_name, *args, **kwargs)
        return self.push_task(task)

    def push_callable(self, func, *args, **kwargs):
        task = FunctionCallTask(self, func, *args, **kwargs)
        return self.push_task(task)

    def push_task(self, task):
        if not isinstance(task, PoolTaskBase):
            raise ValueError(f"{_utils.get_fullname(task)} not a PoolTaskBase")

        with self._lock:
            if not self._quit:
                # _DoneTasksCallbackTask task is high priority and inserted
                # after existing _DoneTasksCallbackTask tasks
                if isinstance(task, _DoneTasksCallbackTask):
                    for idx, tsk in enumerate(self._tasks_inbox):
                        if not isinstance(tsk, _DoneTasksCallbackTask):
                            self._tasks_inbox.insert(idx, task)
                            break
                    else:
                        self._tasks_inbox.append(task)
                else:
                    self._tasks_inbox.append(task)

                self._event.set()
                return True

        return False

    def _thread_main(self):
        while True:
            # should we leave unconditionally?
            with self._lock:
                if self._quit:
                    break

            # wait for an event
            # logger.debug(
            #     "maintenance thread waiting (in:{}, run:{}, done:{})...".format(
            #         len(self._tasks_inbox),
            #         len(self._tasks_running),
            #         len(self._tasks_done)))
            self._event.wait()
            # logger.debug(
            #     "maintenance thread woke up (in:{}, run:{}, done:{})...".format(
            #         len(self._tasks_inbox),
            #         len(self._tasks_running),
            #         len(self._tasks_done)))

            # clear the event flag and check if we should leave
            with self._lock:
                self._event.clear()
                if self._quit:
                    break

            # flush tasks queues
            with self._lock:
                self._thread_main__flush_running_tasks()
                self._thread_main__flush_done_tasks()
                self._thread_main__flush_tasks_inbox()

            # idle? should we leave?
            with self._lock:
                if (self._quit_on_idle and
                        not self._event.is_set() and
                        0 == len(self._tasks_inbox) and
                        0 == len(self._tasks_running) and
                        0 == len(self._tasks_done)):
                    logger.debug("maintenance thread gracefully leaving (idle)")
                    break

        if self._executor is not None:
            self._executor.shutdown(wait=False)

        return 0

    def _thread_main__flush_running_tasks(self):
        # move tasks from *running* to *done* list
        with self._lock:
            must_notify = False

            # note: items are copied to a list so that _tasks_running can be
            # safely modified during the loop
            for future, task in list(self._tasks_running.items()):
                if future.done():
                    self._tasks_done.append(task)
                    del self._tasks_running[future]
                    must_notify = True

            if must_notify:
                self._event.set()

    def _thread_main__flush_done_tasks(self):
        done_tasks = []
        callback = None

        with self._lock:
            if self._tasks_done:
                done_tasks = self._tasks_done
                self._tasks_done = []

                self._event.set()

                callback = self._tasks_done_callback

        if done_tasks and callback is not None:
            # filter out _DoneTasksCallbackTask objects to avoid infinite
            # recursive calls
            done_tasks = [
                t for t in done_tasks
                if not isinstance(t, _DoneTasksCallbackTask)]

            if done_tasks:
                done = _DoneTasksCallbackTask(self, callback, done_tasks)
                self.push_task(done)

    def _thread_main__flush_tasks_inbox(self):
        with self._lock:
            if not self._quit and self._executor is not None:
                # we do not want to flood the ThreadPoolExecutor object as
                # stress tests have shown that it does not deal well with a huge
                # number of tasks in its queue(s) (i.e. 1_000_000)
                #
                # test environment: CPython 3.8.0 x64 on Windows 10
                MAX_SUBMITTED_TASKS = self._max_workers * 2

                if len(self._tasks_running) >= MAX_SUBMITTED_TASKS:
                    # there is no need to _event.set() here because this will be
                    # done upon next task termination, and we know that there is
                    # at least one running task
                    return

                must_notify = False

                while len(self._tasks_inbox) > 0:
                    task = self._tasks_inbox.pop(0)
                    future = task.submit(self._executor)
                    if future is not None:
                        self._tasks_running[future] = task
                        must_notify = True

                        # again, do not mess up with ThreadPoolExecutor!!!
                        if len(self._tasks_running) >= MAX_SUBMITTED_TASKS:
                            break

                if must_notify:
                    self._event.set()

    def _on_future_done(self, future):
        # No need to clean up the _tasks_running list here, this is done by the
        # maintenance thread as long as it gets notified.
        # So keep this method as lightweight and fast as possible.
        with self._lock:
            self._event.set()
