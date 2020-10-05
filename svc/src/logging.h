// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


#ifdef _DEBUG
    #define LOGTRACE(msg, ...)  ::logging::trace(__FILE__, __LINE__, msg, __VA_ARGS__)
#else
    #define LOGTRACE(msg, ...)  ((void)0)
#endif

#ifdef APP_LOGGING_ENABLED
    #ifdef _DEBUG
        #define LOGDEBUG(msg, ...)  ::logging::write(::logging::level_debug, msg, __VA_ARGS__)
        #define LOGINFO(msg, ...)  ::logging::write(::logging::level_info, msg, __VA_ARGS__)
        #define LOGWARNING(msg, ...)  ::logging::write(::logging::level_warning, msg, __VA_ARGS__)
    #else
        #define LOGDEBUG(msg, ...)  ((void)0)
        #define LOGINFO(msg, ...)  ((void)0)
        #define LOGWARNING(msg, ...)  ((void)0)
    #endif
    #define LOGERROR(msg, ...)  ::logging::write(::logging::level_error, msg, __VA_ARGS__)
    #define LOGCRITICAL(msg, ...)  ::logging::write(::logging::level_critical, msg, __VA_ARGS__)
#else
    #define LOGDEBUG(msg, ...)  ((void)0)
    #define LOGINFO(msg, ...)  ((void)0)
    #define LOGWARNING(msg, ...)  ((void)0)
    #define LOGERROR(msg, ...)  ((void)0)
    #define LOGCRITICAL(msg, ...)  ((void)0)
#endif


#ifdef APP_LOGGING_ENABLED

namespace logging
{
    enum level_t
    {
        level_trace,
        level_debug,
        level_info,
        level_warning,
        level_error,
        level_critical,
    };

    void enable_dbgout(bool enable);

    template <
        typename String,
        typename... Args,
        typename Char = fmt::char_t<String>>
    void write(level_t level, const String& format, Args&&... args);

    void write(level_t level, std::string&& msg);
    void write(level_t level, std::wstring&& msg);

#ifdef _DEBUG
    template <
        typename String,
        typename... Args,
        typename Char = fmt::char_t<String>>
    void trace(
        const char* src_file, unsigned src_line,
        const String& format, Args&&... args);

    void trace(const char* src_file, unsigned src_line, std::string&& msg);
    void trace(const char* src_file, unsigned src_line, std::wstring&& msg);
#endif
}  // namespace logging

#include "logging.inl.h"

#endif  // #ifdef APP_LOGGING_ENABLED
