// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once


// compilers enum
#define CIX_COMPILER_CLANG  1
#define CIX_COMPILER_GCC    2
#define CIX_COMPILER_INTEL  3
#define CIX_COMPILER_MSVC   4


// compiler detection
// CAUTION: test order matters since intel's compiler also defines __GNUC__ and
// _MSC_VER macros
#if defined(__INTEL_COMPILER)
    #ifndef __GNUC_PATCHLEVEL__
        #define __GNUC_PATCHLEVEL__  0
    #endif
    #define CIX_COMPILER          CIX_COMPILER_INTEL
    #define CIX_COMPILER_NAME     "intel"
    #define CIX_COMPILER_VERSION  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)  // MMmmPP
    #define CIX_FUNCTION          __func__
    #define CIX_FILE              __BASE_FILE__
    #define CIX_LINE              __LINE__
#elif defined(__clang__)
    #define CIX_COMPILER          CIX_COMPILER_CLANG
    #define CIX_COMPILER_NAME     "clang"
    #define CIX_COMPILER_VERSION  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)  // MMmmPP
    #define CIX_FUNCTION          __func__
    #define CIX_FILE              __BASE_FILE__
    #define CIX_LINE              __LINE__
#elif defined(__GNUC__)
    #ifndef __GNUC_PATCHLEVEL__
        #define __GNUC_PATCHLEVEL__  0
    #endif
    #define CIX_COMPILER          CIX_COMPILER_GCC
    #define CIX_COMPILER_NAME     "gcc"
    #define CIX_COMPILER_VERSION  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)  // MMmmPP
    #define CIX_FUNCTION          __func__  // __PRETTY_FUNCTION__
    #define CIX_FILE              __BASE_FILE__
    #define CIX_LINE              __LINE__
#elif defined(_MSC_VER)
    // * MSVC++ 14.2 _MSC_VER >= 1920 (Visual Studio 2019)
    // * MSVC++ 14.1 _MSC_VER >= 1910 (Visual Studio 2017)
    // * MSVC++ 14.0 _MSC_VER == 1900 (Visual Studio 2015)
    // * MSVC++ 12.0 _MSC_VER == 1800 (Visual Studio 2013)
    // * MSVC++ 11.0 _MSC_VER == 1700 (Visual Studio 2012)
    // * MSVC++ 10.0 _MSC_VER == 1600 (Visual Studio 2010)
    // * MSVC++ 9.0  _MSC_VER == 1500 (Visual Studio 2008)
    // * MSVC++ 8.0  _MSC_VER == 1400 (Visual Studio 2005)
    // * MSVC++ 7.1  _MSC_VER == 1310 (Visual Studio 2003)
    // * MSVC++ 7.0  _MSC_VER == 1300
    // * MSVC++ 6.0  _MSC_VER == 1200
    // * MSVC++ 5.0  _MSC_VER == 1100
    #define CIX_COMPILER          CIX_COMPILER_MSVC
    #define CIX_COMPILER_NAME     "msvc"
    #define CIX_COMPILER_VERSION  _MSC_VER
    #define CIX_FUNCTION          __func__
    #define CIX_FILE              __FILE__
    #define CIX_LINE              __LINE__
#else
    #error unknown compiler
#endif



// platforms enum - linux group
#define CIX_PLATFORM_LINUX    10
#define CIX_PLATFORM_ANDROID  11

// platforms enum - bsg group
#define CIX_PLATFORM_DRAGONFLY  21
#define CIX_PLATFORM_FREEBSD    22
#define CIX_PLATFORM_NETBSD     23
#define CIX_PLATFORM_OPENBSD    24

// platforms enum - windows group
#define CIX_PLATFORM_WINDOWS  30
#define CIX_PLATFORM_CYGWIN   31
#define CIX_PLATFORM_MINGW    32

// platforms enum - apple group
#define CIX_PLATFORM_MACOS          41
#define CIX_PLATFORM_IOS            42
#define CIX_PLATFORM_IOS_SIMULATOR  43

// platforms enum - other unix-like group
#define CIX_PLATFORM_HPUX     51
#define CIX_PLATFORM_SOLARIS  52
#define CIX_PLATFORM_AIX      53


// platform detection
// CAUTION: test order matters
#if defined(__MINGW32__)  // || defined(__MINGW64__)
    #define CIX_PLATFORM_NAME  "mingw"
    #define CIX_PLATFORM       CIX_PLATFORM_MINGW
#elif defined(_WIN32)
    #define CIX_PLATFORM_NAME  "windows"
    #define CIX_PLATFORM       CIX_PLATFORM_WINDOWS
#elif defined(__CYGWIN__)
    #define CIX_PLATFORM_NAME  "cygwin"
    #define CIX_PLATFORM       CIX_PLATFORM_CYGWIN
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        #define CIX_PLATFORM_NAME  "iossim"
        #define CIX_PLATFORM       CIX_PLATFORM_IOS_SIMULATOR
    #elif TARGET_OS_IPHONE
        #define CIX_PLATFORM_NAME  "ios"
        #define CIX_PLATFORM       CIX_PLATFORM_IOS
    #elif TARGET_OS_MAC
        #define CIX_PLATFORM_NAME  "macos"
        #define CIX_PLATFORM       CIX_PLATFORM_MACOS
    #else
        #error unknown apple platform
    #endif
#elif defined(__ANDROID__)
    #define CIX_PLATFORM_NAME  "android"
    #define CIX_PLATFORM       CIX_PLATFORM_ANDROID
#elif defined(__sun) && defined(__SVR4)
    #define CIX_PLATFORM_NAME  "solaris"
    #define CIX_PLATFORM       CIX_PLATFORM_SOLARIS
#elif defined(__hpux)
    #define CIX_PLATFORM_NAME  "hp-ux"
    #define CIX_PLATFORM       CIX_PLATFORM_HPUX
#elif defined(__aix)
    #define CIX_PLATFORM_NAME  "aix"
    #define CIX_PLATFORM       CIX_PLATFORM_AIX
#elif defined(__DragonFly__)
    #define CIX_PLATFORM_NAME  "dragonfly"
    #define CIX_PLATFORM       CIX_PLATFORM_DRAGONFLY
#elif defined(__FreeBSD__)
    #define CIX_PLATFORM_NAME  "freebsd"
    #define CIX_PLATFORM       CIX_PLATFORM_FREEBSD
#elif defined(__OpenBSD__)
    #define CIX_PLATFORM_NAME  "openbsd"
    #define CIX_PLATFORM       CIX_PLATFORM_OPENBSD
#elif defined(__NetBSD__)
    #define CIX_PLATFORM_NAME  "netbsd"
    #define CIX_PLATFORM       CIX_PLATFORM_NETBSD
#elif defined(__linux__)
    #define CIX_PLATFORM_NAME  "linux"
    #define CIX_PLATFORM       CIX_PLATFORM_LINUX
#else
    #error unknown platform
#endif


#if defined(__unix__)
    #define CIX_IS_UNIX  1
#else
    #define CIX_IS_UNIX  0
#endif


#if defined(_POSIX_VERSION)
    #define CIX_IS_POSIX  1
#else
    #define CIX_IS_POSIX  0
#endif



// endianness enum
// * std::endian is C++20
// * exotic endianness not supported
// * values defined here are arbitrary
// * see also <cix/endian.h>
#define CIX_ENDIAN_LITTLE  1234
#define CIX_ENDIAN_BIG     4321


// compile-time endianness detection
// detection logic from rapidjson
// CAUTION: any modification here must be reported to endian.h
#ifndef CIX_ENDIAN
    // detect with GCC 4.6's macro
    #ifdef __BYTE_ORDER__
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
        #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            #define CIX_ENDIAN  CIX_ENDIAN_BIG
        #else
            #error Unknown machine endianness detected. User needs to define CIX_ENDIAN.
        #endif  // __BYTE_ORDER__

    // detect with GLIBC's endian.h
    #elif defined(__GLIBC__)
        #include <endian.h>
        #if (__BYTE_ORDER == __LITTLE_ENDIAN)
            #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
        #elif (__BYTE_ORDER == __BIG_ENDIAN)
            #define CIX_ENDIAN  CIX_ENDIAN_BIG
        #else
            #error Unknown machine endianness detected. User needs to define CIX_ENDIAN.
        #endif  // __GLIBC__

    // detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro
    #elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
        #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
    #elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
        #define CIX_ENDIAN  CIX_ENDIAN_BIG

    // detect with architecture macros
    #elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
        #define CIX_ENDIAN  CIX_ENDIAN_BIG
    #elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
        #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
    #elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
        #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
    // #elif defined(CIX_DOXYGEN_RUNNING)
    //     #define CIX_ENDIAN  CIX_ENDIAN_LITTLE
    #else
        #error unknown endianness
    #endif
#endif  // CIX_ENDIAN
