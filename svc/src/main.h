// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#if defined(_DEBUG) && !defined(APP_LOGGING_ENABLED)
#define APP_LOGGING_ENABLED
#endif

// bootstrap
#include "pch/pch.h"

// namespace shorthands
namespace xpath = ::cix::path;
namespace xstr = ::cix::string;

// base
#include "constants.h"

// utils
#include "utils.h"
#include "logging.h"
#include "inet_ntop.h"

// features
#include "protocol.h"
#include "fdset.h"
#include "socketio.h"
#include "socks_proxy.h"
#include "svc.h"
#include "svc_worker.h"
