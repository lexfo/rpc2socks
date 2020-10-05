// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

namespace cix {


tid_t current_thread_id() noexcept
{
    #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
        return GetCurrentThreadId();
    #else
        return std::this_thread::get_id();
    #endif
}


pid_t current_process_id() noexcept
{
    #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
        return GetCurrentProcessId();
    #else
        return getpid();
    #endif
}


}  // namespace cix
