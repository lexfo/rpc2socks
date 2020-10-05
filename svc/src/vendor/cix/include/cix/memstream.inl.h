// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

template <
    typename T,
    typename std::enable_if_t<
        std::is_integral<T>::value &&
        sizeof(T) >= 2, int>>
inline memstream& memstream::write(const T value)
{
    return this->write(&tmp, sizeof(tmp));
}


template <
    typename T,
    typename std::enable_if_t<
        std::is_integral<T>::value &&
        sizeof(T) >= 2, int>>
inline memstream& memstream::read(T& value)
{
    return this->read(&value, sizeof(value));
}

}  // namespace cix
