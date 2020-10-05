// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#ifdef CIX_FMT_EXTERNAL
    #include <fmt/core.h>
    #include <fmt/format.h>
    // #include <fmt/ranges.h>
#else
    #include "vendor/fmt/core.h"
    #include "vendor/fmt/format.h"
    // #include "fmt/ranges.h"
#endif
