// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

// standard exceptions:
// * logic_error
//     exception class to indicate violations of logical preconditions or class
//     invariants
// * invalid_argument
//     exception class to report invalid arguments
// * domain_error
//     exception class to report domain errors
// * length_error
//     exception class to report attempts to exceed maximum allowed size
// * out_of_range
//     exception class to report arguments outside of expected range
// * runtime_error
//     exception class to indicate conditions only detectable at run time
// * range_error
//     exception class to report range errors in internal computations
// * overflow_error
//     exception class to report arithmetic overflows
// * underflow_error
//     exception class to report arithmetic underflows

#define CIX_THROW_BADARG(msg, ...)    CIX_THROW_STDEXC(::std::invalid_argument, msg, __VA_ARGS__)
#define CIX_THROW_LENGTH(msg, ...)    CIX_THROW_STDEXC(::std::length_error, msg, __VA_ARGS__)
#define CIX_THROW_LOGIC(msg, ...)     CIX_THROW_STDEXC(::std::logic_error, msg, __VA_ARGS__)
#define CIX_THROW_OUTRANGE(msg, ...)  CIX_THROW_STDEXC(::std::out_of_range, msg, __VA_ARGS__)
#define CIX_THROW_RUNTIME(msg, ...)   CIX_THROW_STDEXC(::std::runtime_error, msg, __VA_ARGS__)
#define CIX_THROW_OVERFLOW(msg, ...)  CIX_THROW_STDEXC(::std::overflow_error, msg, __VA_ARGS__)
#define CIX_THROW_UNDERFLOW(msg, ...) CIX_THROW_STDEXC(::std::underflow_error, msg, __VA_ARGS__)

#define CIX_THROW_STDEXC(exc_klass, msg, ...) \
    throw exc_klass(::fmt::format(msg, __VA_ARGS__))

#define CIX_THROW_CRTERR(msg, ...) \
    CIX_THROW_CRTERR_N(errno, msg, __VA_ARGS__)

#define CIX_THROW_CRTERR_N(errno, msg, ...) \
    throw ::std::system_error( \
        ::std::error_code(errno, ::std::system_category()), \
        ::fmt::format(msg, __VA_ARGS__) + ::fmt::format(" (error {})", errno))

#define CIX_THROW_WINERR(msg, ...) \
    CIX_THROW_WINERR_N(::GetLastError(), msg, __VA_ARGS__)

#define CIX_THROW_WINERR_N(errno, msg, ...) \
    throw ::std::runtime_error( \
        ::fmt::format(msg, __VA_ARGS__) + ::fmt::format(" (error {})", errno))

#define CIX_THROW_HRESULT(hresult, msg, ...) \
    throw ::std::runtime_error( \
        ::fmt::format(msg, __VA_ARGS__) + ::fmt::format(" (error {:#08x})", hresult))
