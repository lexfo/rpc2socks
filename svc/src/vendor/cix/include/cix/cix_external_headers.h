// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

// windows headers
#ifdef _WIN32
    #ifndef UNICODE
        #error UNICODE flag required
    #endif
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

// linux extra headers
#ifdef __linux__
    #include <endian.h>
#endif

// posix headers
#ifndef _WIN32
    #include <time.h>
#endif

// windows extra headers
#ifdef _WIN32
    #include <io.h>
#endif
