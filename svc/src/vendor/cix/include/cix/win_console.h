// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {
namespace wincon {

enum init_flags_t
{
    non_intrusive = 0x00,
    can_attach    = 0x01,
    can_create    = 0x02,
};

CIX_IMPLEMENT_ENUM_BITOPS(init_flags_t)

enum style_t : std::uint16_t
{
    style_unset = 0xffff,

    // intensity
    intensity_normal    = 0x0000,
    intensity_bright_fg = 0x0008,
    intensity_bright_bg = 0x0080,

    // foregrounds colors
    fg_black   = intensity_normal | 0x0000,
    fg_blue    = intensity_normal | 0x0100,
    fg_green   = intensity_normal | 0x0200,
    fg_cyan    = intensity_normal | 0x0300,
    fg_red     = intensity_normal | 0x0400,
    fg_magenta = intensity_normal | 0x0500,
    fg_yellow  = intensity_normal | 0x0600,
    fg_grey    = intensity_normal | 0x0700,  // white

    // foregrounds light colors
    fg_lightblack   = intensity_bright_fg | fg_black,
    fg_lightblue    = intensity_bright_fg | fg_blue,
    fg_lightgreen   = intensity_bright_fg | fg_green,
    fg_lightcyan    = intensity_bright_fg | fg_cyan,
    fg_lightred     = intensity_bright_fg | fg_red,
    fg_lightmagenta = intensity_bright_fg | fg_magenta,
    fg_lightyellow  = intensity_bright_fg | fg_yellow,
    fg_lightgrey    = intensity_bright_fg | fg_grey,

    // background colors
    bg_black   = intensity_normal | 0x0000,
    bg_blue    = intensity_normal | 0x1000,
    bg_green   = intensity_normal | 0x2000,
    bg_cyan    = intensity_normal | 0x3000,
    bg_red     = intensity_normal | 0x4000,
    bg_magenta = intensity_normal | 0x5000,
    bg_yellow  = intensity_normal | 0x6000,
    bg_grey    = intensity_normal | 0x7000,  // white

    // background light colors
    bg_lightblack   = intensity_bright_bg | bg_black,
    bg_lightblue    = intensity_bright_bg | bg_blue,
    bg_lightgreen   = intensity_bright_bg | bg_green,
    bg_lightcyan    = intensity_bright_bg | bg_cyan,
    bg_lightred     = intensity_bright_bg | bg_red,
    bg_lightmagenta = intensity_bright_bg | bg_magenta,
    bg_lightyellow  = intensity_bright_bg | bg_yellow,
    bg_lightgrey    = intensity_bright_bg | bg_grey,
};

CIX_IMPLEMENT_ENUM_BITOPS(style_t)

bool init(init_flags_t flags=non_intrusive, std::int16_t min_lines=0);
bool is_init();
void set_title(const std::wstring& title);
bool write(FILE* stream, const std::wstring& msg, style_t style=style_unset);
TCHAR wait_key();
void release();

}  // namespace wincon
}  // namespace cix

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
