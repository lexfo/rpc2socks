// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#ifndef __cplusplus
    #error CIX is a C++ library
#endif


// CIX_INCLUDED (used by detail/ensure_cix.h)
#ifdef CIX_INCLUDED
    #error CIX_INCLUDED already defined?!
#else
    #define CIX_INCLUDED 1
#endif

// CIX_DEBUG
#if defined(__DEBUG) || defined(_DEBUG) || defined(DEBUG)
    // from MSVC documentation: "_DEBUG is defined as 1 when the /LDd, /MDd, or
    // /MTd compiler option is set"
    #ifndef _DEBUG
        #define _DEBUG  1
    #endif

    #ifndef CIX_DEBUG
        #define CIX_DEBUG  1
    #elif (CIX_DEBUG != 1)
        // paranoid check to avoid confusion and so that different DEBUG levels
        // could be defined in the future if desired
        #error macro value inconsistency
    #endif
#endif

// CIX_DEBUG - safety check
#if defined(CIX_DEBUG) && (defined(NDEBUG) || defined(__OPTIMIZE__) || defined(QT_NO_DEBUG))
    #error DEBUG vs. NDEBUG collision detected
#endif



// bootstrap
#include "cix_config.h"
#include "platform.h"

// external headers
#include "cix_external_headers.h"

// fmt library
#include "cix_fmt.h"


// guard header
// must remain before cix own headers
#include "detail/intro.h"


// macros
#include "assert.h"
#include "macros.h"
#include "exceptions.h"
#include "enumbitops.h"

// stdlib utils
#include "std_utils.h"
#include "noncopyable.h"
#include "endian.h"
#include "win_deleters.h"
#include "best_fit.h"
#include "circular.h"

// string utils
#include "string.h"
#include "path.h"
#include "wstr.h"

// stream utils
#include "memstream.h"
#include "memstreambuf.h"

// time utils
#include "monotonic.h"

// prng
#include "random.h"

// hash methods
#include "crc32.h"

// threading
#include "thread.h"
#include "win_thread.h"
#include "win_recursive_mutex.h"
#include "lock_guard.h"
#include "win_namedpipe_server.h"

// windows console mini wrapper
#include "win_console.h"


#include "detail/outro.h"  // must remain last
