// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#ifdef _WIN32

namespace cix {

template <typename T = HLOCAL>
struct local_dter
{
    void operator()(T* ptr) const
    {
        if (ptr)
            LocalFree(ptr);  // reinterpret_cast<HLOCAL>(ptr));
    }
};

template <typename T>
using unique_local = std::unique_ptr<T, local_dter<T>>;


template <typename T = HGLOBAL>
struct global_dter
{
    void operator()(T* ptr) const
    {
        if (ptr)
            GlobalFree(ptr);  // reinterpret_cast<HGLOBAL>(ptr));
    }
};

template <typename T>
using unique_global = std::unique_ptr<T, global_dter<T>>;


template <typename T = void>
struct cotaskmem_dter
{
    void operator()(T* ptr) const
    {
        if (ptr)
            CoTaskMemFree(ptr);
    }
};

template <typename T>
using unique_cotaskmem = std::unique_ptr<T, cotaskmem_dter<T>>;


template <typename T = std::remove_pointer_t<SC_HANDLE>>
struct sc_handle_dter
{
    void operator()(T* ptr) const
    {
        if (ptr)
            CloseServiceHandle(ptr);
    }
};

using unique_sc_handle =
    std::unique_ptr<
        std::remove_pointer_t<SC_HANDLE>,
        sc_handle_dter<std::remove_pointer_t<SC_HANDLE>>>;


template <typename T = HANDLE>
struct event_source_dter
{
    void operator()(T* ptr) const
    {
        if (ptr)
            DeregisterEventSource(ptr);
    }
};

using unique_event_source = std::unique_ptr<HANDLE, event_source_dter<HANDLE>>;

}  // namespace cix

#endif  // #ifdef _WIN32
