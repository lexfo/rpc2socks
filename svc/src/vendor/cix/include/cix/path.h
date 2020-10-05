// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {
namespace path {

template <typename Char>
static constexpr Char native_sep =
    #ifdef _WIN32
        Char('\\');
    #else
        Char('/');
    #endif

template <typename Char>
static constexpr Char unix_sep = Char('/');

template <typename Char>
static constexpr Char win_sep = Char('\\');

template <typename Char>
static constexpr std::basic_string_view<Char> all_sep(Char("/\\"));

template <typename Char>
constexpr bool is_sep(Char c) noexcept;

template <typename Char>
constexpr bool is_drive_letter(Char c) noexcept;

template <typename String>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    bool>
is_absolute(const String& path) noexcept;


// template <
//     typename String,
//     typename Char = string::char_t<String>>
// std::enable_if_t<
//         string::is_string_viewable_v<String>,
//     std::basic_string_view<Char>>
// root(const String& path) noexcept;

// template <
//     typename String,
//     typename Char = string::char_t<String>>
// std::enable_if_t<
//         string::is_string_viewable_v<String>,
//     std::basic_string_view<Char>>
// nonroot(const String& path) noexcept;

// basename()
template <
    typename String,
    typename Char = string::char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string_view<Char>>
name(const String& path) noexcept;

template <
    typename String,
    typename Char = string::char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string_view<Char>>
strip_ext(const String& path) noexcept;

template <
    typename String,
    typename Char = string::char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string_view<Char>>
strip_all_ext(const String& path) noexcept;

// title = name without .ext
template <
    typename String,
    typename Char = string::char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string_view<Char>>
title(const String& path) noexcept;


// join (variadic)
template <
    typename String,
    typename... Args,
    typename Char = char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string<Char>>
join(
    Char sep,
    const String& head,
    Args&&... args) noexcept;

// join (from a container of strings)
template <
    typename Container,
    typename Char = string::is_container_of_strings<Container>::char_type>
std::enable_if_t<
        string::is_container_of_strings_v<Container>,
    std::basic_string<Char>>
join(
    Char sep,
    const Container& elements) noexcept;


// join_native (variadic)
template <
    typename String,
    typename... Args,
    typename Char = char_t<String>>
std::enable_if_t<
        string::is_string_viewable_v<String>,
    std::basic_string<Char>>
join_native(
    const String& head,
    Args&&... args) noexcept;

// join_native (from a container of strings)
template <
    typename Container,
    typename Char = string::is_container_of_strings<Container>::char_type>
std::enable_if_t<
        string::is_container_of_strings_v<Container>,
    std::basic_string<Char>>
join_native(
    const Container& elements) noexcept;

}  // namespace path
}  // namespace cix

#include "path.inl.h"
