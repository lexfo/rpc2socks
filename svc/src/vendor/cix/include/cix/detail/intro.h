// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

// CAUTION: Every change made in this file must be applied to "outro.h" as well
//
// The push_macro() pragma is non-standard and seems to be an MSVC extension
// originally, however it is supported by (recent) versions of clang and GCC at
// least

#ifdef _WIN32

    #ifdef min
        #define _CIX__PUSHED_MACRO__min
        #pragma push_macro("min")
        #undef min
    #endif

    #ifdef max
        #define _CIX__PUSHED_MACRO__max
        #pragma push_macro("max")
        #undef max
    #endif

    // #ifdef realloc
    //     #define _CIX__PUSHED_MACRO__realloc
    //     #pragma push_macro("realloc")
    //     #undef realloc
    // #endif

    // #ifdef free
    //     #define _CIX__PUSHED_MACRO__free
    //     #pragma push_macro("free")
    //     #undef free
    // #endif

#endif
