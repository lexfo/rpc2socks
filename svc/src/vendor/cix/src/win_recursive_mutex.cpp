// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {

win_recursive_mutex::win_recursive_mutex()
    : m_handle{nullptr}
{
    m_handle = CreateMutex(nullptr, FALSE, nullptr);
    if (!m_handle)
        CIX_THROW_WINERR("win_recursive_mutex's CreateMutex failed");
}

win_recursive_mutex::~win_recursive_mutex()
{
    CloseHandle(m_handle);
    m_handle = nullptr;
}

void win_recursive_mutex::lock()
{
    const DWORD res = WaitForSingleObject(m_handle, INFINITE);
    switch (res)
    {
        case WAIT_OBJECT_0:
            return;

        case WAIT_TIMEOUT:  // duh?!
        case WAIT_ABANDONED:  // CAUTION: potential data corruption
        default:
            assert(0);
            return;

        case WAIT_FAILED:
            CIX_THROW_WINERR("win_recursive_mutex::lock failed");
    }
}

bool win_recursive_mutex::try_lock()
{
    const DWORD res = WaitForSingleObject(m_handle, 0);
    switch (res)
    {
        case WAIT_OBJECT_0:
            return true;

        case WAIT_TIMEOUT:
            return false;

        case WAIT_ABANDONED:  // CAUTION: potential data corruption
            assert(0);
            return true;

        case WAIT_FAILED:
            CIX_THROW_WINERR("win_recursive_mutex::try_lock failed");

        default:
            assert(0);
            return false;
    }
}

void win_recursive_mutex::unlock()
{
#ifdef CIX_DEBUG
    if (!ReleaseMutex(m_handle))
    {
        const DWORD error = GetLastError();
        // CIX_LOGERR("failed to release mutex (error {})", error);
        assert(0);
        SetLastError(error);
    }
#else
    ReleaseMutex(m_handle);
#endif
}

}  // namespace cix

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
