// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

enum class endian
{
    little = CIX_ENDIAN_LITTLE,
    big = CIX_ENDIAN_BIG,
    native = CIX_ENDIAN,
};

static_assert(
    endian::native == endian::little || endian::native == endian::big,
    "mixed endianness not supported");



template <typename T>
inline constexpr
std::enable_if_t<endian::native != endian::big, T>
native_to_big(T value) noexcept
{ return swap(value); }

template <typename T>
inline constexpr
std::enable_if_t<endian::native == endian::big, T>
native_to_big(T value) noexcept
{ return value; }

template <typename T>
inline constexpr
std::enable_if_t<endian::native != endian::little, T>
native_to_little(T value) noexcept
{ return swap(value); }

template <typename T>
inline constexpr
std::enable_if_t<endian::native == endian::little, T>
native_to_little(T value) noexcept
{ return value; }



template <typename T>
inline constexpr T
big_to_native(T value) noexcept
{ return native_to_big(value); }

template <typename T>
inline constexpr T
little_to_native(T value) noexcept
{ return native_to_little(value); }



template <typename T>
inline constexpr T
hton(T value) noexcept
{ return native_to_big(value); }

template <typename T>
inline constexpr T
ntoh(T value) noexcept
{ return big_to_native(value); }



template <typename T>
inline constexpr
std::enable_if_t<sizeof(T) == 1, T>
swap(T value) noexcept
{
    return value;
}


template <typename T>
inline constexpr
std::enable_if_t<sizeof(T) == 2, T>
swap(T value) noexcept
{
    #if (CIX_COMPILER == CIX_COMPILER_CLANG) || (CIX_COMPILER == CIX_COMPILER_GCC)
        const auto res = __builtin_bswap16(*reinterpret_cast<std::uint16_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif (CIX_COMPILER == CIX_COMPILER_INTEL)
        const auto res = _bswap16(*reinterpret_cast<std::uint16_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif defined(_MSVC_VER)
        const auto res = _byteswap_ushort(*reinterpret_cast<std::uint16_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #else
        register std::uint16_t tmp = *reinterpret_cast<std::uint16_t*>(&value);
        tmp = (tmp << 8) | ((tmp >> 8) & 0x00ff);
        return *reinterpret_cast<T*>(&tmp);
    #endif
}


template <typename T>
inline constexpr
std::enable_if_t<sizeof(T) == 4, T>
swap(T value) noexcept
{
    #if (CIX_COMPILER == CIX_COMPILER_CLANG) || (CIX_COMPILER == CIX_COMPILER_GCC)
        const auto res = __builtin_bswap32(*reinterpret_cast<std::uint32_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif (CIX_COMPILER == CIX_COMPILER_INTEL)
        const auto res = _bswap(*reinterpret_cast<std::uint32_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif defined(_MSVC_VER)
        const auto res = _byteswap_ulong(*reinterpret_cast<std::uint32_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #else
        register std::uint32_t tmp = *reinterpret_cast<std::uint32_t*>(&value);
        tmp =
            ((tmp << 24) & 0xff000000u) |
            ((tmp <<  8) & 0x00ff0000u) |
            ((tmp >>  8) & 0x0000ff00u) |
            ((tmp >> 24) & 0x000000ffu);
        return *reinterpret_cast<T*>(&tmp);
    #endif
}


template <typename T>
inline constexpr
std::enable_if_t<sizeof(T) == 8, T>
swap(T value) noexcept
{
    #if (CIX_COMPILER == CIX_COMPILER_CLANG) || (CIX_COMPILER == CIX_COMPILER_GCC)
        const auto res = __builtin_bswap64(*reinterpret_cast<std::uint64_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif (CIX_COMPILER == CIX_COMPILER_INTEL)
        const auto res = _bswap64(*reinterpret_cast<std::uint64_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #elif defined(_MSVC_VER)
        const auto res = _byteswap_uint64(*reinterpret_cast<std::uint64_t*>(&value));
        return *reinterpret_cast<T*>(&res);

    #else
        register std::uint64_t tmp = *reinterpret_cast<std::uint64_t*>(&value);
        tmp =
            ((tmp << 56) & 0xff00000000000000u) |
            ((tmp << 40) & 0x00ff000000000000u) |
            ((tmp << 24) & 0x0000ff0000000000u) |
            ((tmp <<  8) & 0x000000ff00000000u) |
            ((tmp >>  8) & 0x00000000ff000000u) |
            ((tmp >> 24) & 0x0000000000ff0000u) |
            ((tmp >> 40) & 0x000000000000ff00u) |
            ((tmp >> 56) & 0x00000000000000ffu);
        return *reinterpret_cast<T*>(&tmp);
    #endif
}


template <typename T, std::size_t N = sizeof(T)>
inline constexpr void swap_inplace(T& value) noexcept
{
    auto* ptr = reinterpret_cast<std::uint8_t*>(&value);
    std::reverse(ptr, ptr + N);
}


}  // namespace cix
