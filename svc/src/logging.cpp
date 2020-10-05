// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"

#ifdef APP_LOGGING_ENABLED

namespace logging {

namespace detail
{
    static std::mutex log_mutex;
    static bool enable_dbgout = false;

    static const wchar_t* level_to_string(level_t level)
    {
        switch (level)
        {
            case level_trace: return L"TRACE";
            case level_debug: return L"DBG";
            case level_info: return L"INF";
            case level_warning: return L"WRN";
            case level_error: return L"ERR";
            case level_critical: return L"XXX";

            default:
                assert(0);
                return L"???";
        }
    }

    static ::cix::wincon::style_t level_to_wincon_style(level_t level)
    {
        switch (level)
        {
            case level_trace: return ::cix::wincon::fg_green;
            case level_debug: return ::cix::wincon::fg_cyan;
            case level_info: return ::cix::wincon::fg_lightgrey;
            case level_warning: return ::cix::wincon::fg_yellow;
            case level_error: return ::cix::wincon::fg_red;
            case level_critical: return ::cix::wincon::fg_magenta;

            default:
                assert(0);
                return ::cix::wincon::fg_grey;
        }
    }
}


void enable_dbgout(bool enable)
{
    std::scoped_lock guard(detail::log_mutex);
    detail::enable_dbgout = enable;
}


void write(level_t level, std::string&& msg)
{
    logging::write(level, std::move(xstr::widen_utf8_lenient(msg)));
}


void write(level_t level, std::wstring&& msg)
{
    SYSTEMTIME st;

    GetLocalTime(&st);

    auto output = xstr::fmt(
        L"{:02}:{:02}:{:02}.{:03} [{}] {}\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        detail::level_to_string(level), std::move(msg));

    std::scoped_lock guard(detail::log_mutex);

    if (detail::enable_dbgout)
        OutputDebugStringW(output.c_str());

    if (stderr)
    {
        ::cix::wincon::write(
            stderr, output, detail::level_to_wincon_style(level));
    }
}


#ifdef _DEBUG
void trace(const char* src_file, unsigned src_line, std::string&& msg)
{
    logging::trace(
        src_file, src_line,
        std::move(xstr::widen_utf8_lenient(msg)));
}
#endif


#ifdef _DEBUG
void trace(const char* src_file, unsigned src_line, std::wstring&& msg)
{
    SYSTEMTIME st;

    GetLocalTime(&st);

    // CAUTION: widen_utf8_lenient() assumes src_file is utf8-encoded

    auto prefix = xstr::fmt(
        L"{}({}): ",
        xstr::widen_utf8_lenient(src_file), src_line);

    auto output = xstr::fmt(
        L"{}{:02}:{:02}:{:02}.{:03} [{}] {}\n",
        prefix,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        detail::level_to_string(level_trace), std::move(msg));

    // TODO / FIXME: guarantee call order

    std::scoped_lock guard(detail::log_mutex);

    OutputDebugStringW(output.c_str());

    if (stderr)
    {
        output.erase(0, prefix.length());
        ::cix::wincon::write(
            stderr, output, detail::level_to_wincon_style(level_trace));
    }
}
#endif

}  // namespace logging

#endif  // #ifdef APP_LOGGING_ENABLED
