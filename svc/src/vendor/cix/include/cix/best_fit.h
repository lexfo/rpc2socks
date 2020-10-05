// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

template <typename T, typename = void>
struct best_fit { };


template <typename T>
struct best_fit<T*>
{
    typedef std::uintptr_t unsigned_t;
    typedef std::intptr_t signed_t;
};


// template <>
// struct best_fit<std::int32_t>
// {
//     typedef std::uint32_t unsigned_t;
//     typedef std::int32_t signed_t;
// };

// template <>
// struct best_fit<std::uint32_t>
// {
//     typedef std::uint32_t unsigned_t;
//     typedef std::int32_t signed_t;
// };


// template <>
// struct best_fit<std::int64_t>
// {
//     typedef std::uint64_t unsigned_t;
//     typedef std::int64_t signed_t;
// };

// template <>
// struct best_fit<std::uint64_t>
// {
//     typedef std::uint64_t unsigned_t;
//     typedef std::int64_t signed_t;
// };


template <typename T>
struct best_fit<
    T,
    std::enable_if_t<
        std::is_standard_layout_v<T> &&
        !std::is_pointer_v<T> &&
        sizeof(T) == sizeof(std::uint32_t)>>
{
    typedef std::uint32_t unsigned_t;
    typedef std::int32_t signed_t;
};

template <typename T>
struct best_fit<
    T,
    std::enable_if_t<
        std::is_standard_layout_v<T> &&
        !std::is_pointer_v<T> &&
        sizeof(T) == sizeof(std::uint64_t)>>
{
    typedef std::uint64_t unsigned_t;
    typedef std::int64_t signed_t;
};


}  // namespace cix
