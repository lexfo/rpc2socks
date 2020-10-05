// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// debug?
#if defined(__DEBUG) || defined(_DEBUG) || defined(DEBUG)
    #ifndef _DEBUG
        #define _DEBUG
    #endif
#endif

// windows base headers
#ifdef _WIN32
    #ifndef UNICODE
        #error UNICODE flag required
    #endif

    // <winsock2.h> must be included before <windows.h> because <windows.h>
    // defaults to windsock1 unless WIN32_LEAN_AND_MEAN is defined
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>

    #if defined(__DEBUG) || defined(_DEBUG) || defined(DEBUG)
        #ifdef _MSC_VER
            #include <cstdlib>
            #include <crtdbg.h>
        #else
            #include <cassert>
        #endif
    #endif

    #include <windows.h>
    #include <versionhelpers.h>
#endif

// c headers
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <clocale>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

// c++ headers
#include <algorithm>
// #include <bit>  // C++20
#include <chrono>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <locale>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

// c++ containers
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// c++ streams
#include <streambuf>

// c++ threading
#include <atomic>
#include <mutex>
#include <thread>

// windows extra headers
#ifdef _WIN32
    #include <io.h>
    #include <shellapi.h>
#endif

// unix extra headers
#if defined(__linux__)
    #include <endian.h>
#endif

// remove namespace polluting macros defined in windows headers
// * min() and max() may not be defined depending on whether NOMINMAX has been
//   defined
// * realloc() macro collides with Qt
#ifdef _WIN32
    #ifdef min
        #undef min
    #endif
    #ifdef max
        #undef max
    #endif
#endif

// cix header
#include <cix/cix>
