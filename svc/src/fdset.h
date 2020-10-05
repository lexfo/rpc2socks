// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


class fdset_t
{
public:
    enum : std::size_t
    {
        // this is just for the sake of having a max limit and avoiding
        // theorical overflow, since the maximum number supported sockets is
        // implementation-dependent
        //
        // related: http://smallvoid.com/article/winnt-tcpip-max-limit.html
        max_sockets = static_cast<std::size_t>(std::numeric_limits<int>::max()),
    };

public:
    fdset_t();
    ~fdset_t() = default;

    bool empty() const;
    std::size_t size() const;
    bool has(SOCKET socket) const;

    bool register_socket(SOCKET socket);
    void unregister_socket(SOCKET socket);
    void unregister_all();

    fd_set* build_native();

private:
    std::set<SOCKET> m_set;
    std::vector<std::uint8_t> m_struct;
    bool m_size_changed;
};
