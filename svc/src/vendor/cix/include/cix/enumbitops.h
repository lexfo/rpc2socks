// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

// bitwise operators on enum values
#define CIX_IMPLEMENT_ENUM_BITOPS(T) \
    inline constexpr T operator&(T lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        return static_cast<T>(static_cast<SubT>(lhs) & static_cast<SubT>(rhs)); \
    } \
    inline constexpr T operator|(T lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        return static_cast<T>(static_cast<SubT>(lhs) | static_cast<SubT>(rhs)); \
    } \
    inline constexpr T operator^(T lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        return static_cast<T>(static_cast<SubT>(lhs) ^ static_cast<SubT>(rhs)); \
    } \
    inline constexpr T operator~(T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        return static_cast<T>(~static_cast<SubT>(rhs)); \
    } \
    inline T& operator&=(T& lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        lhs = static_cast<T>(static_cast<SubT>(lhs) & static_cast<SubT>(rhs)); \
        return lhs; \
    } \
    inline T& operator|=(T& lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        lhs = static_cast<T>(static_cast<SubT>(lhs) | static_cast<SubT>(rhs)); \
        return lhs; \
    } \
    inline T& operator^=(T& lhs, T rhs) \
    { \
        using SubT = ::std::underlying_type_t<T>; \
        lhs = static_cast<T>(static_cast<SubT>(lhs) ^ static_cast<SubT>(rhs)); \
        return lhs; \
    }
