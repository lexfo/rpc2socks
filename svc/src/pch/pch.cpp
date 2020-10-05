// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "pch.h"

#ifdef APP_ENABLE_SERVICE
    #pragma message("Compiling as a Service")
#else
    #pragma message("Compiling as a Console Application")
#endif
