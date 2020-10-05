// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {
namespace wincon {

namespace detail
{
    static constexpr WORD ATTR_UNSET = static_cast<WORD>(-1);

    static std::mutex console_mutex;
    static bool is_init = false;
    static bool must_free = false;
    static bool reopened_stdin = false;
    static bool reopened_stdout = false;
    static bool reopened_stderr = false;
    static WORD orig_attributes = ATTR_UNSET;


    static bool redirect_io()
    {
        bool must_reset_iostream = false;
        FILE* fp;

        if (!reopened_stdin &&
            GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE &&
            0 == freopen_s(&fp, "CONIN$", "r", stdin))
        {
            setvbuf(stdin, nullptr, _IONBF, 0);
            reopened_stdin = true;
            must_reset_iostream = true;
        }

        if (!reopened_stdout &&
            GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE &&
            0 == freopen_s(&fp, "CONOUT$", "w", stdout))
        {
            setvbuf(stdout, nullptr, _IONBF, 0);
            reopened_stdout = true;
            must_reset_iostream = true;
        }

        if (!reopened_stderr &&
            GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE &&
            0 == freopen_s(&fp, "CONOUT$", "w", stderr))
        {
            setvbuf(stderr, nullptr, _IONBF, 0);
            reopened_stderr = true;
            must_reset_iostream = true;
        }

        if (must_reset_iostream)
        {
            // ::std::ios::sync_with_stdio(true);

            // ::std::wcout.clear();
            // ::std::cout.clear();
            // ::std::wcerr.clear();
            // ::std::cerr.clear();
            // ::std::wclog.clear();
            // ::std::clog.clear();
            // ::std::wcin.clear();
            // ::std::cin.clear();
        }

        return must_reset_iostream;  // at least one stream redirected
    }


    static void unredirect_io()
    {
        FILE* fp;

        if (reopened_stdin && 0 == freopen_s(&fp, "NUL:", "r", stdin))
            setvbuf(stdin, nullptr, _IONBF, 0);

        if (reopened_stdout && 0 == freopen_s(&fp, "NUL:", "r", stdout))
            setvbuf(stdout, nullptr, _IONBF, 0);

        if (reopened_stderr && 0 == freopen_s(&fp, "NUL:", "r", stderr))
            setvbuf(stderr, nullptr, _IONBF, 0);

        reopened_stdin = false;
        reopened_stdout = false;
        reopened_stderr = false;
    }


    static WORD get_current_attributes()
    {
        const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if (handle &&
            handle != INVALID_HANDLE_VALUE &&
            GetConsoleScreenBufferInfo(handle, &csbi))
        {
            return csbi.wAttributes;
        }

        return ATTR_UNSET;
    }


    static void ensure_buffer_lines(std::int16_t min_lines)
    {
        const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if (handle &&
            handle != INVALID_HANDLE_VALUE &&
            GetConsoleScreenBufferInfo(handle, &csbi) &&
            csbi.dwSize.Y < min_lines)
        {
            csbi.dwSize.Y = min_lines;
            SetConsoleScreenBufferSize(handle, csbi.dwSize);
        }
    }


    static WORD style_to_attributes(style_t style)
    {
        // from <consoleapi2.h>
        //   #define FOREGROUND_BLUE      0x0001
        //   #define FOREGROUND_GREEN     0x0002
        //   #define FOREGROUND_RED       0x0004
        //   #define FOREGROUND_INTENSITY 0x0008
        //   #define BACKGROUND_BLUE      0x0010
        //   #define BACKGROUND_GREEN     0x0020
        //   #define BACKGROUND_RED       0x0040
        //   #define BACKGROUND_INTENSITY 0x0080

        WORD attr = 0;

        if (style & intensity_bright_fg)
            attr |= FOREGROUND_INTENSITY;

        if (style & intensity_bright_bg)
            attr |= BACKGROUND_INTENSITY;

        // background color
        attr |= (style & 0x7000) >> 8;

        // foreground color
        attr |= (style & 0x0700) >> 8;

        return attr;
    }

}  // namespace detail


bool init(init_flags_t flags, std::int16_t min_lines)
{
    std::scoped_lock guard(detail::console_mutex);

    if (detail::is_init)
        return true;

    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle && handle != INVALID_HANDLE_VALUE)
    {
        // CAUTION: no necessarily a TTY!
        detail::must_free = false;
        detail::is_init = true;
    }

    if (!detail::is_init &&
        (flags & init_flags_t::can_attach) &&
        AttachConsole(ATTACH_PARENT_PROCESS))
    {
        detail::must_free = true;
        detail::is_init = true;
    }

    if (!detail::is_init && (flags & init_flags_t::can_create))
    {
        if (!AllocConsole())
        {
#ifdef _DEBUG
            const auto error = GetLastError();
            CIX_UNVAR(error);
#endif
            assert(0);
            return false;
        }

        detail::must_free = true;
        detail::is_init = true;
    }

    bool redirected = false;

    if (detail::is_init)
    {
        if (min_lines > 0)
            detail::ensure_buffer_lines(min_lines);

        detail::orig_attributes = detail::get_current_attributes();

        if (detail::must_free)
            redirected = detail::redirect_io();
        else
            redirected = true;
    }

    return detail::is_init && redirected;
}


bool is_init()
{
    std::scoped_lock guard(detail::console_mutex);
    return detail::is_init;
}


void set_title(const std::wstring& title)
{
    std::scoped_lock guard(detail::console_mutex);
    SetConsoleTitleW(title.c_str());
}


bool write(FILE* stream, const std::wstring& msg, style_t style)
{
    if (!stream)
    {
        assert(0);
        return false;
    }

    std::scoped_lock guard(detail::console_mutex);

    HANDLE handle = INVALID_HANDLE_VALUE;

    // if (!detail::is_init)
    //     return false;

    // if (stream != stderr && stream != stdout)
    // {
    //     assert(0);
    //     stream = stderr;
    // }

    if (style != style_unset)
    {
        const int fd = _fileno(stream);

        // do not try to apply attributes if output is not a tty
        if (fd != -1 && _isatty(fd))
        {
            handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
            if (!handle ||
                handle == INVALID_HANDLE_VALUE ||
                handle == HANDLE(-2))
            {
                handle = INVALID_HANDLE_VALUE;
            }
        }

        if (handle == INVALID_HANDLE_VALUE)
            style = style_unset;
    }

    if (style != style_unset &&
        !SetConsoleTextAttribute(handle, detail::style_to_attributes(style)))
    {
        style = style_unset;
    }

    fputws(msg.c_str(), stream);

    // restore previous style
    if (style != style_unset)
    {
        const WORD orig_attr =
            (detail::orig_attributes != detail::ATTR_UNSET) ?
            detail::orig_attributes : 0;

        SetConsoleTextAttribute(handle, orig_attr);
    }

    return true;
}


TCHAR wait_key()
{
    std::scoped_lock guard(detail::console_mutex);

    if (!detail::is_init)
        return false;

    const HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    TCHAR ch = 0;
    DWORD chars_read = 0;

    if (!hin || hin == INVALID_HANDLE_VALUE)
        return 0;

    if (!GetConsoleMode(hin, &mode))
        { assert(0); return 0; }

    if (!SetConsoleMode(hin, 0))
        { assert(0); return 0; }

    WaitForSingleObject(hin, INFINITE);
    ReadConsoleW(hin, &ch, 1, &chars_read, nullptr);
    SetConsoleMode(hin, mode);

    return ch;
}


void release()
{
    std::scoped_lock guard(detail::console_mutex);

    if (detail::is_init)
    {
        detail::unredirect_io();

        if (detail::must_free)
        {
            FreeConsole();
            detail::must_free = false;
        }

        detail::is_init = false;
    }
}


}  // namespace wincon
}  // namespace cix

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
