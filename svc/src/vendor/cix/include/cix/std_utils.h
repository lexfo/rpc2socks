// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"


// Trick from cppreference.com and the *fmt* library:
//
// An enable_if<> helper to be used in template parameters which results in much
// shorter symbols: https://godbolt.org/z/sWw4vP. Extra parentheses are needed
// to workaround a bug in MSVC 2019 (see fmt#1140 and fmt#1186).
#define CIX_ENABLE_IF(...)  ::std::enable_if_t<(__VA_ARGS__), int> = 0


namespace cix {


// number of elements in a regular C array
template <typename T, std::size_t s>
inline constexpr std::size_t countof(T (&arr)[s])
{
    CIX_UNVAR(arr);
    return s;
}


// number of elements in a standard container
template <typename T>
inline constexpr std::size_t countof(T&& arr)
{
    return arr.size();
}


// forward arg as value of decayed type
template <class T>
inline typename std::decay<T>::type decay_copy(T&& arg)
{
    return std::forward<T>(arg);
}


/// Implements ``*reinterpret_cast<DestT*>(&source)``, of which behavior is
/// undefined by the C++ specification.
template <class DestT, class SourceT>
inline DestT bit_cast(const SourceT& source)
{
    // reminder: std::is_pod depecrated as of C++20
    static_assert(std::is_standard_layout_v<SourceT>, "SourceT not POD");
    static_assert(std::is_standard_layout_v<DestT>, "DestT not POD");
    static_assert(
        sizeof(DestT) == sizeof(SourceT),
        "source and dest sizes mismatch");

    DestT dest;
    std::memcpy(&dest, &source, sizeof(dest));
    return dest;
}


/// Check whether ``T`` is an integer type, that is an integral type that is not
/// a bool, a char nor a wchar_t.
template <typename T>
using is_integer = ::std::integral_constant<
    bool,
    ::std::is_integral<T>::value &&
        !::std::is_same<T, bool>::value &&
        !::std::is_same<T, char>::value &&
        !::std::is_same<T, wchar_t>::value>;

/// A shortcut to :member:`is_integer<T>::value`
template <typename T>
using is_integer_v = typename is_integer<T>::value;


/// Find a weak pointer in a container of weak pointers
template <typename InputIt, typename T>
InputIt find_weak_ptr(InputIt first, InputIt last, const std::weak_ptr<T>& weak)
{
    if (weak.expired())
    {
        assert(0);
        return last;
    }
    return std::find_if(first, last,
        [&weak](std::iterator_traits<InputIt>::reference ref) {
            assert(!weak.expired());
            return
                !ref.expired() && !weak.expired() &&
                !ref.owner_before(weak) && !weak.owner_before(ref);
        });
}


/// Match a shared pointer in a container of weak pointers
template <typename InputIt, typename T>
InputIt find_weak_ptr(InputIt first, InputIt last, const std::shared_ptr<T>& shared)
{
    if (!shared)
    {
        assert(0);
        return last;
    }
    return std::find_if(first, last,
        [&shared](std::iterator_traits<InputIt>::reference ref) {
            assert(!shared);
            return
                !ref.expired() && !shared &&
                !ref.owner_before(shared) && !shared.owner_before(ref);
        });
}


/**
    /rst
    A "less" functor for ``std::shared_ptr<T>``.

    .. note::
        Ensure ``T::operator<(const T&)`` is implemented before using this.
    /endrst
*/
template <typename T>
struct shared_less
{
    bool operator()(
        const std::shared_ptr<T>& lhs,
        const std::shared_ptr<T>& rhs) const
    {
        if (lhs && rhs)
            return (lhs == rhs) ? false : (*lhs < *rhs);
        else if (!lhs && !rhs)
            return false;
        return !lhs;
    }
};


template <typename MapT>
bool map_equal(const MapT& lhs, const MapT& rhs)
{
    return
        lhs.size() == rhs.size() &&
        std::equal(lhs.begin(), lhs.end(), rhs.begin());
}


template <typename MapT>
bool map_keys_equal(const MapT& lhs, const MapT& rhs)
{
    return
        lhs.size() == rhs.size() &&
        std::equal(lhs.begin(), lhs.end(), rhs.begin(),
            [](const auto a, const auto b) { return a.first == b.first });
}


template<typename TK, typename... TT>
std::set<TK> map_copy_keys_to_set(const std::map<TK, TT...>& m)
{
    std::set<TK> set;
    for (const auto& it : m)
        set.insert(it.first);
    return set;
}


template<typename TK, typename... TT>
std::vector<TK> map_copy_keys_to_vec(const std::map<TK, TT...>& m)
{
    std::vector<TK> vec;
    vec.reserve(m.size());
    for (const auto& it : m)
        vec.push_back(it.first);
    return vec;
}


template <typename KeyT, typename ValueT, typename... TT>
std::vector<ValueT> map_copy_values(const std::map<KeyT, ValueT, TT...>& m)
{
    std::vector<ValueT> vec;
    vec.reserve(m.size());
    for (const auto& it : m)
        vec.push_back(it.second);
    return vec;
}

}  // namespace cix
