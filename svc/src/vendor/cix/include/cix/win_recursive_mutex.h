// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {

/**
    /rst
    An implementation of std::recursive_mutex for Windows platforms.

    It internally uses a Windows' Mutex object. ``CRITICAL_SECTION`` objects are
    *slightly* faster but are less secure than a Mutex for the following
    reasons:

    * A thread that terminates without releasing a ``CRITICAL_SECTION`` it owns
      will leave it in an undefined state.
    * Additionally, if a ``CRITICAL_SECTION`` object is deleted while being
      still owned, the state of the thread(s) waiting for ownership of the
      deleted critical section is undefined.
    /endrst
*/
class win_recursive_mutex
{
public:
    typedef HANDLE native_handle_type;

public:
    win_recursive_mutex();
    win_recursive_mutex(const win_recursive_mutex&) = delete;
    win_recursive_mutex(win_recursive_mutex&&) = delete;
    ~win_recursive_mutex();

    void lock();
    bool try_lock();
    void unlock();
    inline native_handle_type native_handle() { return m_handle; }

    win_recursive_mutex& operator=(const win_recursive_mutex&) = delete;
    win_recursive_mutex& operator=(win_recursive_mutex&&) = delete;

private:
    native_handle_type m_handle;
};


}  // namespace cix

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
