// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


// An auto-sized i/o handler for SOCKET objects based on select()
//
// * SOCKET objects are "registered" once connected.
// * Compatibility with win2k/3 was a major requirement, far before
//   performances :)
// * select() is used to poll sockets.
// * Two threads are created instead of a single one so that we can wait for
//   both SOCKET and EVENT objects concurrently - what select() does not allow.
// * An EVENT object is used internally to trigger a write() call to a socket
//   (see socketio::write_thread).
//
// CAUTION:
// * SOCKET handles passed to register_socket() method are expected to be
//   BLOCKING (that is, FIONBIO option set to 0)
// * OOB data not supported
class socketio
{
public:
    typedef std::uint8_t byte_t;
    typedef std::vector<byte_t> bytes_t;

    struct listener_t
    {
        virtual void on_socketio_recv(SOCKET socket, bytes_t&& packet) = 0;
        virtual void on_socketio_disconnected(SOCKET socket) = 0;
    };

private:
    // The start size of the common input buffer
    // * This class re-uses the same buffer for every socket-level recv()
    //   operations. Once a recv() call returned, the received bytes are copied
    //   into a newly allocated buffer with the exact required size.
    // * Note that recv() calls are not simultaneous.
    // * The common input buffer may grow over time in case a recv() call
    //   indicates the buffer is too small (i.e. WSAEMSGSIZE error)
    static constexpr std::size_t input_buffer_start_size = 64 * 1024;

public:
    socketio();
    ~socketio() = default;

    void set_stop_event(HANDLE stop_event);
    void set_listener(std::shared_ptr<listener_t> listener);

    void launch();
    void register_socket(SOCKET socket);
    bool send(SOCKET socket, bytes_t&& packet);
    void disconnect_and_unregister_socket(SOCKET socket);
    void unregister_socket(SOCKET socket);
    void join();

    static void milliseconds_to_timeval(long milliseconds, TIMEVAL& tv);
    static int enable_socket_nonblocking_mode(SOCKET sock, bool enable);

private:
    void read_thread();
    void read_thread__cleanup(const fd_set& fds_except, fd_set& fds_read);
    void read_thread__do(bytes_t& buffer, fd_set& fds_read);
    void read_thread__do(bytes_t& buffer, SOCKET socket);

    void write_thread();
    void write_thread__do();
    void write_thread__do(SOCKET socket);

    void unregister_non_sockets(fd_set& fds);

    void notify_recv(SOCKET socket, bytes_t&& packet);
    void notify_disconnected(SOCKET socket);

    static std::size_t send_impl(SOCKET socket, const bytes_t& packet);

private:
    std::recursive_mutex m_mutex;
    std::unique_ptr<std::thread> m_read_thread;
    std::unique_ptr<std::thread> m_write_thread;
    HANDLE m_stop_event;

    fdset_t m_fdset_read;
    fdset_t m_fdset_write;
    fdset_t m_fdset_except;

    std::map<SOCKET, std::list<bytes_t>> m_write_queue;
    HANDLE m_write_event;

    std::weak_ptr<listener_t> m_listener;
};
