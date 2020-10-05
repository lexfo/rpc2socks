// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


fdset_t::fdset_t()
    : m_size_changed{true}
{
}


bool fdset_t::empty() const
{
    return m_set.empty();
}


std::size_t fdset_t::size() const
{
    return m_set.size();
}


bool fdset_t::has(SOCKET socket) const
{
    return m_set.find(socket) != m_set.end();
}


bool fdset_t::register_socket(SOCKET socket)
{
    if (m_set.size() >= fdset_t::max_sockets)
    {
        assert(0);
        return false;
    }

    if (m_set.insert(socket).second)
        m_size_changed = true;

    return true;
}


void fdset_t::unregister_socket(SOCKET socket)
{
    if (m_set.erase(socket) > 0)
        m_size_changed = true;
}


void fdset_t::unregister_all()
{
    if (!m_set.empty())
        m_size_changed = true;

    m_set.clear();
}


fd_set* fdset_t::build_native()
{
    // select() alters the content of m_struct's buffer so rebuild its content
    // unconditionally.
    //
    // FIXME: this could be improved by pre-building a vector of SOCKET to
    // memcpy to m_struct. Vector would be re-built upon effective unregister*()
    // call only.

    struct _fdset_t
    {
        unsigned int fd_count;
        SOCKET fd_array[1];
    };

    if (m_size_changed)
    {
        const std::size_t required_size =
            m_set.empty() ?
            sizeof(_fdset_t) :
            (sizeof(_fdset_t) + ((m_set.size() - 1) * sizeof(SOCKET)));

        m_struct.resize(required_size);

        m_size_changed = false;
    }

    auto* fds = reinterpret_cast<_fdset_t*>(m_struct.data());

    if (m_set.empty())
    {
        fds->fd_count = 0;
        fds->fd_array[0] = INVALID_SOCKET;
    }
    else
    {
        fds->fd_count = 0;
        for (const auto socket : m_set)
            fds->fd_array[fds->fd_count++] = socket;
    }

    return reinterpret_cast<fd_set*>(fds);
}
