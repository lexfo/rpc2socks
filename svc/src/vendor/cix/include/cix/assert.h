// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"


// CIX_DEBUG_BREAK
#ifndef CIX_DEBUG_BREAK
    #if defined(__clang__) || defined(__GNUC__)
        #define CIX_DEBUG_BREAK()  __builtin_trap()
    #elif defined(_MSC_VER)
        #define CIX_DEBUG_BREAK()  __debugbreak()
    // #else
    //     #error compiler not supported
    #endif
#endif


// assertions
#ifndef CIX_ASSERT
    #ifdef CIX_DEBUG
        #ifdef CIX_DEBUG_BREAK
            #define CIX_ASSERT(condition) \
                (void)((!!(condition)) || (CIX_DEBUG_BREAK(), 0))

            #ifdef CIX_OVERRIDE_ASSERT
                #ifdef assert
                    #undef assert
                #endif
                #define assert CIX_ASSERT
            #endif
        #else
            #define CIX_ASSERT assert
        #endif
    #else
        #define CIX_ASSERT(x) ((void)0)

        #ifdef CIX_OVERRIDE_ASSERT
            #ifdef assert
                #undef assert
            #endif
            #define assert CIX_ASSERT
        #endif
    #endif
#endif
