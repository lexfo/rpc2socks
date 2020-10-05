// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

// use this macro to make your code uncompilable until you remove it
#define CIX_TODO  static_assert(false, "TODO!!!")
#define CIXTODO   CIX_TODO

// util macros
#define CIX_VERBATIM(x)       x
#define CIX_STRINGIZE(x)      _CIX_STRINGIZE_(x)
#define CIX_CONCAT(a,b)       _CIX_CONCAT_(a,b)
#define CIX_CONCAT3(a,b,c)    _CIX_CONCAT3_(a,b,c)
#define CIX_CONCAT4(a,b,c,d)  _CIX_CONCAT4_(a,b,c,d)

// macros stubs (internal use)
#define _CIX_STRINGIZE_(x)      #x
#define _CIX_CONCAT_(a,b)       a##b
#define _CIX_CONCAT3_(a,b,c)    a##b##c
#define _CIX_CONCAT4_(a,b,c,d)  a##b##c##d


// unused variable / argument
#ifdef UNREFERENCED_PARAMETER
    // UNREFERENCED_PARAMETER, as defined in <winnt.h>
    #define CIX_UNVAR     UNREFERENCED_PARAMETER
#else
    #define CIX_UNVAR(x)  (x)
#endif
