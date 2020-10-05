// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


// exit codes
enum exit_t : int
{
    APP_EXITCODE_OK = 0,
    APP_EXITCODE_ERROR = 1,  // generic code for failure
    APP_EXITCODE_ARG = 2,  // invalid command line argument
    APP_EXITCODE_RUNNING = 3,  // an instance of this app is running already
    APP_EXITCODE_API = 4,
};
