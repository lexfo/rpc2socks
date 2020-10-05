// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

namespace cix {


// ticks_now
#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS && WINVER < 0x0600  // pre-Vista
    inline ticks_t ticks_now()
    {
        return GetTickCount();
    }
#elif CIX_PLATFORM == CIX_PLATFORM_WINDOWS && WINVER >= 0x0600  // Vista+
    inline ticks_t ticks_now()
    {
        return GetTickCount64();
    }
#elif !defined(CLOCK_MONOTONIC) && (CIX_PLATFORM == CIX_PLATFORM_HPUX || CIX_PLATFORM == CIX_PLATFORM_SOLARIS)
    inline ticks_t ticks_now()
    {
        return ::gethrtime() / 1'000'000;
    }
#endif


inline ticks_t ticks_elapsed(ticks_t start)
{
    return ticks_elapsed(start, ticks_now());
}


inline ticks_t ticks_to_go(ticks_t start, ticks_t end)
{
    return ticks_to_go(start, end, ticks_now());
}

}  // namespace cix
