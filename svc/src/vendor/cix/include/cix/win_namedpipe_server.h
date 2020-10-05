// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#if defined(CIX_ENABLE_WIN_NAMEDPIPE_SERVER) && (CIX_PLATFORM == CIX_PLATFORM_WINDOWS)

namespace cix {

// a threaded server-side of a named pipe
// it manages multiple instances - clients - with a single dedicated thread by
// using completion routines
class win_namedpipe_server :
    public std::enable_shared_from_this<win_namedpipe_server>
{
public:
    // default size of the internal I/O buffer
    static constexpr DWORD io_buffer_default_size = 64 * 1024;  // xkcd221

    // maximum number of pending writes per instance_t at kernel level
    // * this value controls the maximum number of pending writes to a pipe
    //   instance_t, at kernel level
    // * if this limit is reached, win_namedpipe_server will wait for the
    //   client-side to complete its read operations - which flush the write-end
    //   of the pipe - until there is less than *max_pending_kernel_writes*
    //   remaining, before calling WriteFileEx() again
    // * this causes win_namedpipe_server::instance_t's own output queue
    //   (*m_output* member) to grow in case the user of this class writes
    //   faster than what the client-side is able to read
    // * this value can be set to 0 (null) to disable this soft-limit, in which
    //   case write operations are pushed onto kernel's own queue
    static constexpr std::size_t max_pending_kernel_writes = 10;

    // returned by get_output_queue_size() on error
    static constexpr std::size_t invalid_queue_size =
        std::numeric_limits<std::size_t>::max();

    enum flags_t : unsigned
    {
        flag_message       = 0x01,  // defaults to byte mode
        flag_accept_remote = 0x02,
        flag_impersonate   = 0x04,  // null dacl

        flag_default = 0,
    };

    typedef cix::best_fit<HANDLE>::unsigned_t instance_token_t;

    typedef std::vector<std::uint8_t> bytes_t;

    struct listener_t
    {
        virtual void on_namedpipe_connected(
            std::shared_ptr<win_namedpipe_server> pipe,
            win_namedpipe_server::instance_token_t pipe_instance_token) = 0;

        virtual void on_namedpipe_recv(
            std::shared_ptr<win_namedpipe_server> pipe,
            win_namedpipe_server::instance_token_t pipe_instance_token,
            win_namedpipe_server::bytes_t&& packet) = 0;

        virtual void on_namedpipe_sent(
            std::shared_ptr<win_namedpipe_server> pipe,
            win_namedpipe_server::instance_token_t pipe_instance_token,
            win_namedpipe_server::bytes_t&& packet,
            std::size_t output_queue_size) = 0;

        virtual void on_namedpipe_closed(
            std::shared_ptr<win_namedpipe_server> pipe,
            win_namedpipe_server::instance_token_t pipe_instance_token) = 0;
    };

private:
    class instance_t : public std::enable_shared_from_this<instance_t>
    {
    public:
        struct overlapped_t
        {
            overlapped_t() = delete;
            overlapped_t(
                    std::shared_ptr<instance_t> instance_,
                    bytes_t& packet_)
                : ol{}
                , instance(instance_)
                { packet.swap(packet_); }
            overlapped_t(
                    std::shared_ptr<instance_t> instance_,
                    bytes_t&& packet_)
                : ol{}
                , instance(instance_)
                , packet(std::move(packet_))
                { }
            ~overlapped_t() = default;

            OVERLAPPED ol;  // CAUTION: must remain first
            std::weak_ptr<instance_t> instance;
            bytes_t packet;  // input or output data
        };

    public:
        instance_t() = delete;
        instance_t(
            std::shared_ptr<win_namedpipe_server> parent,
            HANDLE pipe);
        ~instance_t();

        instance_token_t token() const;
        std::shared_ptr<win_namedpipe_server> parent() const;
        bool orphan() const;
        std::size_t output_queue_size() const;

        void proceed();
        bool write(bytes_t&& packet);
        void disconnect();
        void close();

        void on_read(std::shared_ptr<overlapped_t> ol);
        void on_written(std::shared_ptr<overlapped_t> ol);

    private:
        // properties
        mutable std::recursive_mutex m_mutex;
        std::weak_ptr<win_namedpipe_server> m_parent;
        instance_token_t m_token;
        HANDLE m_pipe;

        // state
        std::shared_ptr<overlapped_t> m_olread;
        std::map<overlapped_t*, std::weak_ptr<overlapped_t>> m_olwrites;
        std::queue<bytes_t> m_output;
    };

public:
    win_namedpipe_server();
    ~win_namedpipe_server();

    void set_flags(flags_t flags);
    void set_path(const std::wstring& pipe_path);
    void set_listener(std::shared_ptr<listener_t> listener);  // can be null

    void launch();

    bool send(instance_token_t instance_token, bytes_t&& packet);
    bool send_to_first(bytes_t&& packet);
    std::size_t broadcast_packet(bytes_t&& packet);

    // returns *invalid_queue_size* on error
    std::size_t get_output_queue_size(instance_token_t instance_token) const;

    bool disconnect_instance(instance_token_t instance_token);

    void stop();

private:
    void maintenance_thread();
    HANDLE open_and_listen(OVERLAPPED* ol, bool* out_connecting);
    void create_instance(HANDLE pipe_handle);
    void handle_proceed_event();

    void notify_read(instance_token_t token, bytes_t&& packet);
    void notify_written(
        instance_token_t token, bytes_t&& packet,
        std::size_t output_queue_size);
    void notify_closed(instance_token_t token);

    static void WINAPI apc_completed_read(
        DWORD error, DWORD bytes_read, LPOVERLAPPED ol);
    static void WINAPI apc_completed_write(
        DWORD error, DWORD bytes_written, LPOVERLAPPED ol);

private:
    mutable std::recursive_mutex m_mutex;
    std::wstring m_path;
    std::weak_ptr<listener_t> m_listener;
    std::unique_ptr<std::thread> m_thread;
    HANDLE m_stop_event;
    HANDLE m_connect_event;
    HANDLE m_proceed_event;
    flags_t m_flags;
    std::map<instance_token_t, std::shared_ptr<instance_t>> m_instances;
    std::set<instance_token_t> m_proceed;

private:
    static std::mutex ms_overlapped_registry_mutex;
    static std::map<
        instance_t::overlapped_t*,
        std::shared_ptr<instance_t::overlapped_t>>
            ms_overlapped_registry;
};

CIX_IMPLEMENT_ENUM_BITOPS(win_namedpipe_server::flags_t)

}  // namespace cix

#endif  // #if defined(CIX_ENABLE_WIN_NAMEDPIPE_SERVER) && (CIX_PLATFORM == CIX_PLATFORM_WINDOWS)
