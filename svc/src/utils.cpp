// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"

namespace utils {

std::wstring module_path(HMODULE hmodule)
{
    std::wstring dest(MAX_PATH, 0);

    SetLastError(0);
    const DWORD res = GetModuleFileNameW(hmodule, dest.data(), MAX_PATH);
    if (res == 0 || res >= MAX_PATH)
        return {};

    dest.erase(static_cast<decltype(dest)::size_type>(res));

    return dest;
}

}  // namespace utils
