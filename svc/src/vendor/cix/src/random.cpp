// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

namespace cix {
namespace random {

namespace detail
{
    // return the number of microseconds elapsed since 1601-01-01 UTC
    static std::uint64_t now_microseconds() noexcept
    {
        #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
            FILETIME ft;
            ULARGE_INTEGER uli;

            GetSystemTimeAsFileTime(&ft);

            uli.LowPart = ft.dwLowDateTime;
            uli.HighPart = ft.dwHighDateTime;

            return uli.QuadPart / 10;

        #elif defined(CLOCK_REALTIME)
            struct timespec ts;

            if (0 != clock_gettime(CLOCK_REALTIME, &ts))
                return cix::ticks_now();

            return
                static_cast<std::uint64_t>(tick.tv_sec) * 1'000'000 +
                static_cast<std::uint64_t>(tick.tv_nsec) / 1'000 +
                11'644'473'600'000'000ull;

        #else
            #error platform not supported
        #endif
    }

    static std::uint64_t cputime() noexcept
    {
        #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
            LARGE_INTEGER li;
            QueryPerformanceCounter(&li);
            return static_cast<std::uint64_t>(li.QuadPart);

        #elif defined(CLOCK_MONOTONIC)
            struct timespec ts;

            if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
                return cix::ticks_now();

            return
                static_cast<std::uint64_t>(tick.tv_sec) * 1'000'000'000ull +
                static_cast<std::uint64_t>(tick.tv_nsec);

        #else
            #error platform not supported
        #endif
    }

    // MurmurHash3's 64bit finalizer
    // https://github.com/aappleby/smhasher/wiki/MurmurHash3
    static std::uint64_t mmh3_avalanche(std::uint64_t h) noexcept
    {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;

        return h;
    }
}


std::uint64_t generate_seed64_a() noexcept
{
    std::uint64_t tmp = detail::now_microseconds();
    tmp = detail::mmh3_avalanche(tmp);
    return detail::mmh3_avalanche(tmp | 1);
}


std::uint64_t generate_seed64_b() noexcept
{
    std::uint64_t tmp = detail::cputime();
    tmp =
        detail::mmh3_avalanche(tmp) +
        (detail::mmh3_avalanche(cix::current_thread_id()) << 1);
    return detail::mmh3_avalanche(tmp | 1);
}



//******************************************************************************



fast::fast() noexcept
    : m_state{generate_seed64_a(), generate_seed64_b()}
{
    // warmup
    for (std::size_t idx = 0; idx < 10; ++idx)
        this->xorshift128();
}

fast::fast(std::uint64_t state0, std::uint64_t state1) noexcept
    : m_state{state0, state1}
{
}

void fast::get_state(std::uint64_t* state0, std::uint64_t* state1) noexcept
{
    assert(state0);
    assert(state1);

    if (state0)
        *state0 = m_state[0];

    if (state1)
        *state1 = m_state[1];
}

void fast::set_state(std::uint64_t state0, std::uint64_t state1) noexcept
{
    m_state[0] = state0;
    m_state[1] = state1;
}

void fast::xorshift128() noexcept
{
    std::uint64_t s1 = m_state[0];
    const std::uint64_t s0 = m_state[1];

    m_state[0] = s0;

    s1 ^= s1 << 23;  // a
    s1 ^= s1 >> 17;  // b
    s1 ^= s0;
    s1 ^= s0 >> 26;  // c

    m_state[1] = s1;
}


}  // namespace random
}  // namespace cix
