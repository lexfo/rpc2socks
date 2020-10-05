// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {
namespace random {


// An implementation of a variation of the xorshift128+ pseudo random number
// generator, with the same A, B and C parameters than the V8 JavaScript engine.
//
// The default constructor calls both generate_seed64_a() and
// generate_seed64_b() functions to initialize the internal state.
//
// next64() is the *native* method. Other "next*" methods follow the
// recommendation made by the author of xorshift128+ to use the higher bits.
class fast
{
public:
    fast() noexcept;
    fast(std::uint64_t state0, std::uint64_t state1) noexcept;
    ~fast() = default;

    std::uint64_t next64() noexcept
    {
        this->xorshift128();
        return m_state[0] + m_state[1];
    }

    std::uint32_t next32() noexcept
    {
        this->xorshift128();
        return static_cast<std::uint32_t>(
            (m_state[0] + m_state[1]) >> (64 - 32));
    }

    std::uint16_t next16() noexcept
    {
        this->xorshift128();
        return static_cast<std::uint16_t>(
            (m_state[0] + m_state[1]) >> (64 - 16));
    }

    std::uint8_t next8() noexcept
    {
        this->xorshift128();
        return static_cast<std::uint8_t>(
            (m_state[0] + m_state[1]) >> (64 - 8));
    }

    double next_double() noexcept
    {
        // exponent for double values for [1.0 .. 2.0)
        // this is the 64-bits representation of 1.0 (IEEE754)
        static const std::uint64_t exponent_bits =
            std::uint64_t{0x3ff0000000000000};

        this->xorshift128();
        const std::uint64_t random = (m_state[0] >> 12) | exponent_bits;
        return cix::bit_cast<double>(random) - 1.0;
    }

    void get_state(std::uint64_t* state0, std::uint64_t* state1) noexcept;
    void set_state(std::uint64_t state0, std::uint64_t state1) noexcept;

private:
    void xorshift128() noexcept;

private:
    std::uint64_t m_state[2];
};


// utility functions to generate a seed using miscellaneous
// informations from the environment
std::uint64_t generate_seed64_a() noexcept;
std::uint64_t generate_seed64_b() noexcept;


}  // namespace random
}  // namespace cix
