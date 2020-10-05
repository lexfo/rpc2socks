// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

// thread id type
typedef cix::best_fit<::std::thread::id>::unsigned_t tid_t;

// process id type
#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
    typedef cix::best_fit<DWORD>::unsigned_t pid_t;
#else
    typedef cix::best_fit<::pid_t>::unsigned_t pid_t;
#endif


tid_t current_thread_id() noexcept;
pid_t current_process_id() noexcept;


}  // namespace cix
