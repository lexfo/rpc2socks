// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#ifdef _WIN32

    #ifdef _CIX__PUSHED_MACRO__min
        #undef _CIX__PUSHED_MACRO__min
        #pragma pop_macro("min")
    #endif

    #ifdef _CIX__PUSHED_MACRO__max
        #undef _CIX__PUSHED_MACRO__max
        #pragma pop_macro("max")
    #endif

    #ifdef _CIX__PUSHED_MACRO__realloc
        #undef _CIX__PUSHED_MACRO__realloc
        #pragma pop_macro("realloc")
    #endif

    #ifdef _CIX__PUSHED_MACRO__free
        #undef _CIX__PUSHED_MACRO__free
        #pragma pop_macro("free")
    #endif

#endif
