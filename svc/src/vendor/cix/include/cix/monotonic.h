// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

// a monotonic clock with millisecond precision

namespace cix {

// ticks_t (milliseconds)
#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS && WINVER < 0x0600  // pre-Vista
    typedef std::uint32_t ticks_t;
#else
    typedef std::uint64_t ticks_t;
#endif

static constexpr ticks_t ticks_second = 1000;
static constexpr ticks_t ticks_minute = 60 * ticks_second;
static constexpr ticks_t ticks_hour = 60 * ticks_minute;
static constexpr ticks_t ticks_day = 24 * ticks_hour;

ticks_t ticks_now();
ticks_t ticks_elapsed(ticks_t start);
ticks_t ticks_elapsed(ticks_t start, ticks_t now);
ticks_t ticks_to_go(ticks_t start, ticks_t end);
ticks_t ticks_to_go(ticks_t start, ticks_t end, ticks_t now);
std::string ticks_to_string(ticks_t milliseconds);
std::wstring ticks_to_wstring(ticks_t milliseconds);

}  // namespace cix


#include "monotonic.inl.h"
