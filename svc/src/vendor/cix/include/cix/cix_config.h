// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once


//------------------------------------------------------------------------------
// Allow cix's own CIX_ASSERT() macro (defined in cix/assert.h) to override
// standard assert().
//
// Only when CIX_DEBUG gets defined by <cix/cix.h>.
// Ignored if CIX_ASSERT is already defined.

// #define CIX_OVERRIDE_ASSERT


//------------------------------------------------------------------------------
// Uncomment to use your own copy of the fmt library instead of cix's copy.
// In this case cix will try to #include <fmt/format.h>.

// #define CIX_FMT_EXTERNAL


//------------------------------------------------------------------------------
// cix::wstr is a fully featured utility class for string manipulation, derived
// from std::wstring.
//
// It is meant to be superseded by the cix::string and cix::path namespaces, a
// more generic and modern C++ approach with a strong emphasis on
// meta-programming (e.g. templates, SFINAE) and the use of string views
// (std::basic_string_view).
//
// cix::wstr still offers more features than the cix::string-cix::path duo and
// may feel more convenient to use. However it is less compliant with modern
// C++, not generic, and is less optimized in some places (speed and memory).
// Due to string views not being used for instance.
//
// Uncomment this to enable the cix::wstr class and its associated features.

// #define CIX_ENABLE_WSTR


//------------------------------------------------------------------------------
// cix::win_namedpipe_server is a class that supports serving multiple instances
// of a named pipe in an asynchronous full-duplex fashion.
//
// Uncomment this to enable the cix::win_namedpipe_server

// #define CIX_ENABLE_WIN_NAMEDPIPE_SERVER
