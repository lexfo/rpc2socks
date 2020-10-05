// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


namespace detail
{
    static std::mutex _rand_mutex;
    static cix::random::fast _rand_gen;

    inline static u_int fdset_rand(u_int elements)
    {
        std::scoped_lock lock(_rand_mutex);
        return _rand_gen.next32() % elements;  // okay'ish
    }
}


socketio::socketio()
    : m_stop_event{nullptr}
    , m_write_event{nullptr}
{
    m_write_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_write_event)
        CIX_THROW_WINERR("failed to create sio stop event");
}


void socketio::set_stop_event(HANDLE stop_event)
{
    std::scoped_lock lock(m_mutex);

    m_stop_event = stop_event;
}


void socketio::set_listener(std::shared_ptr<listener_t> listener)
{
    std::scoped_lock lock(m_mutex);

    m_listener = listener;
}


void socketio::launch()
{
    std::scoped_lock lock(m_mutex);

    if (!m_stop_event)
    {
        assert(0);
        return;
    }

    if (WAIT_TIMEOUT != WaitForSingleObject(m_stop_event, 0))
        return;

    if (m_read_thread || m_write_thread)
    {
        assert(0);
        return;
    }

    m_write_thread = std::make_unique<std::thread>(
        std::bind(&socketio::write_thread, this));

    m_read_thread = std::make_unique<std::thread>(
        std::bind(&socketio::read_thread, this));
}


void socketio::join()
{
    std::scoped_lock lock(m_mutex);

    assert(WAIT_TIMEOUT != WaitForSingleObject(m_stop_event, 0));

    if (m_read_thread)
    {
        if (m_read_thread->joinable())
            m_read_thread->join();

        m_read_thread.reset();
    }

    if (m_write_thread)
    {
        if (m_write_thread->joinable())
            m_write_thread->join();

        m_write_thread.reset();
    }
}


void socketio::register_socket(SOCKET socket)
{
    if (GetFileType(reinterpret_cast<HANDLE>(socket)) != FILE_TYPE_PIPE)
    {
        assert(0);
        return;
    }

    std::scoped_lock lock(m_mutex);

    m_fdset_read.register_socket(socket);
    m_fdset_except.register_socket(socket);
}


bool socketio::send(SOCKET socket, bytes_t&& packet)
{
    if (socket == INVALID_SOCKET)
        return false;

    std::scoped_lock lock(m_mutex);

    if (!m_fdset_read.has(socket))
        return false;

    if (!m_write_thread || !m_write_thread->joinable())
        return false;

    if (GetFileType(reinterpret_cast<HANDLE>(socket)) != FILE_TYPE_PIPE)
    {
        this->unregister_socket(socket);
        return false;
    }

    // cannot blindly try-insert() due to the use of std::move(), and we want
    // to reserve() the output queue

    auto it = m_write_queue.find(socket);

    if (it != m_write_queue.end())
    {
        it->second.push_back(std::move(packet));
    }
    else
    {
        auto pair = m_write_queue.insert(
            std::make_pair(socket, std::list<bytes_t>()));

        // pair.first->second.reserve(10);
        pair.first->second.push_back(std::move(packet));
    }

    m_fdset_write.register_socket(socket);
    SetEvent(m_write_event);

    return true;
}


void socketio::disconnect_and_unregister_socket(SOCKET socket)
{
    cix::lock_guard lock(m_mutex);

    this->unregister_socket(socket);

    lock.unlock();

    // FIXME: dirty hack; shutdown is a blocking call so enable
    // non-blocking mode. Ideally, this should be handled by a separate thread.
    socketio::enable_socket_nonblocking_mode(socket, true);
    shutdown(socket, SD_BOTH);
    Sleep(50);
    closesocket(socket);
}


void socketio::unregister_socket(SOCKET socket)
{
    std::scoped_lock lock(m_mutex);

    m_fdset_read.unregister_socket(socket);
    m_fdset_write.unregister_socket(socket);
    m_fdset_except.unregister_socket(socket);

    m_write_queue.erase(socket);
    if (m_write_queue.empty())
        ResetEvent(m_write_event);
}


void socketio::read_thread()
{
    fd_set* fds_read = nullptr;
    fd_set* fds_except = nullptr;
    TIMEVAL tv;
    bytes_t input_buffer;

    auto check_stop =
        [this](DWORD wait_time) {
            return WaitForSingleObject(m_stop_event, wait_time) != WAIT_TIMEOUT;
        };

    CIX_THREAD_SET_NAME_STATIC(GetCurrentThreadId(), "socketio[read]");

    // * one input buffer that will never shrink
    // * it is the one passed to recv() calls
    // * we still have to allocate memory on a per-recv() call basis, but at
    //   least the right amount of memory per received packet is allocated
    //   (exact amount depends on std::vector implementation though)
    // * see read_thread__do() method for more info
    input_buffer.resize(socketio::input_buffer_start_size);

    for (;;)
    {
        if (check_stop(0))
            break;

        // fdset_t::build_native() changes its internal state so protect the
        // call with a mutex even though the returned result is not thread-safe
        //
        // CAUTION: fdset_t::build_native() is not thread-safe
        {
            std::scoped_lock lock(m_mutex);

            fds_read = m_fdset_read.build_native();
            fds_except = m_fdset_except.build_native();

            assert(fds_read->fd_count == fds_except->fd_count);
        }

        if (!fds_read->fd_count)
        {
            if (check_stop(200))
                break;
            continue;
        }

        socketio::milliseconds_to_timeval(1000, tv);

        const int selres = select(
            static_cast<int>(fds_read->fd_count),  // "ignored"
            fds_read, nullptr, fds_except, &tv);

        if (selres == SOCKET_ERROR)
        {
            const auto wsaerror = WSAGetLastError();

            assert(0);

            if (wsaerror == WSAENOTSOCK)  // "This Should Never Happen" (c)
            {
                assert(0);  // TEST
                this->unregister_non_sockets(*fds_read);
            }

            // avoid consuming too much CPU in an infinite loop
            const DWORD wait_time = wsaerror == WSAENETDOWN ? 500 : 100;
            if (check_stop(wait_time))
                break;
        }
        else if (selres == 0)
        {
            // select() timed out
        }
        else
        {
            this->read_thread__cleanup(*fds_except, *fds_read);
            this->read_thread__do(input_buffer, *fds_read);
        }
    }
}


void socketio::read_thread__cleanup(const fd_set& fds_except, fd_set& fds_read)
{
    std::scoped_lock lock(m_mutex);

    for (u_int idx = 0; idx < fds_except.fd_count; ++idx)
    {
        const auto socket = fds_except.fd_array[idx];

        // ensure socket has not been unregistered during the call to select()
        if (!m_fdset_read.has(socket))
            continue;

        // unreference this socket from the "read" set if needed so that
        // read_thread__do() does not recv() it
        for (u_int idx_read = 0; idx_read < fds_read.fd_count; ++idx_read)
        {
            if (fds_read.fd_array[idx_read] == socket)
            {
                fds_read.fd_array[idx_read] = INVALID_SOCKET;
                break;
            }
        }

        this->notify_disconnected(socket);
        this->unregister_socket(socket);
    }
}


void socketio::read_thread__do(bytes_t& buffer, fd_set& fds_read)
{
    u_int count = 0;
    u_int idx = detail::fdset_rand(fds_read.fd_count);

    for (; count < fds_read.fd_count; ++count, ++idx)
    {
        if (idx >= fds_read.fd_count)
            idx = 0;

        const auto socket = fds_read.fd_array[idx];

        // ensure socket has not been unregistered during the call to select()
        if (!m_fdset_read.has(socket))
            continue;

        if (socket != INVALID_SOCKET)  // because of read_thread__cleanup()
            this->read_thread__do(buffer, socket);
    }
}


void socketio::read_thread__do(bytes_t& buffer, SOCKET socket)
{
    // paranoid check
    if (buffer.size() < socketio::input_buffer_start_size)
        buffer.resize(socketio::input_buffer_start_size);

    std::size_t bytes_recv = 0;

    for (;;)
    {
        assert(bytes_recv < buffer.size());

        const int to_recv = static_cast<int>(std::min(
            buffer.size() - bytes_recv,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));

        WSASetLastError(0);

        const int res = recv(
            socket, reinterpret_cast<char*>(buffer.data() + bytes_recv),
            to_recv, 0);

        const auto wsaerror = WSAGetLastError();

        if (res == SOCKET_ERROR)
        {
            if (wsaerror == WSANOTINITIALISED ||
                wsaerror == WSAENETDOWN ||
                wsaerror == WSAENOTCONN ||
                wsaerror == WSAENETRESET ||
                wsaerror == WSAENOTSOCK ||
                wsaerror == WSAESHUTDOWN ||
                wsaerror == WSAECONNABORTED ||
                wsaerror == WSAETIMEDOUT ||
                wsaerror == WSAECONNRESET)
            {
                this->notify_disconnected(socket);
                this->unregister_socket(socket);
            }

            // SecureZeroMemory(buffer.data(), buffer.size());

            return;
        }
        else if (res == 0)  // connection shutdown
        {
            this->notify_disconnected(socket);
            this->unregister_socket(socket);

            // SecureZeroMemory(buffer.data(), buffer.size());

            return;
        }
        else if (res > 0)
        {
            bytes_recv += static_cast<std::size_t>(res);

            if (wsaerror == WSAEMSGSIZE)
            {
                buffer.resize(
                    buffer.size() + socketio::input_buffer_start_size);
                continue;
            }

            break;
        }
        else
        {
            assert(0);  // "We Should Never Get Here" (c)
            break;
        }
    }

    if (bytes_recv > 0)
    {
        bytes_t packet(
            buffer.begin(),
            std::next(buffer.begin(), bytes_recv));

        // SecureZeroMemory(buffer.data(), buffer.size());

        this->notify_recv(socket, std::move(packet));
    }
}


void socketio::write_thread()
{
    const HANDLE events[] = { m_stop_event, m_write_event };

    CIX_THREAD_SET_NAME_STATIC(GetCurrentThreadId(), "socketio[write]");

    for (;;)
    {
        const auto wait_res = WaitForMultipleObjects(
            static_cast<DWORD>(cix::countof(events)),
            reinterpret_cast<const HANDLE*>(&events),
            FALSE, INFINITE);

        if (wait_res == WAIT_OBJECT_0)  // stop event
        {
            break;
        }
        else if (wait_res == WAIT_OBJECT_0 + 1)  // write event
        {
            this->write_thread__do();
        }
        else
        {
#ifdef _DEBUG
            const auto error = GetLastError();
            // LOGERROR(
            //     "named pipe server worker failed to enter into waiting mode "
            //     "(result {}; error {})",
            //     wait_res, error);
            CIX_UNVAR(error);
            assert(0);
#endif
            break;
        }
    }
}


void socketio::write_thread__do()
{
    fd_set* fds = nullptr;
    TIMEVAL tv;

    auto check_stop =
        [this](DWORD wait_time) {
            return WaitForSingleObject(m_stop_event, wait_time) != WAIT_TIMEOUT;
        };

    for (;;)
    {
        if (check_stop(0))
            return;

        // fdset_t::build_native() changes its internal state so protect the
        // call with a mutex even though the returned result is not thread-safe
        //
        // CAUTION: fdset_t::build_native() is not thread-safe
        {
            std::scoped_lock lock(m_mutex);
            fds = m_fdset_write.build_native();
        }

        if (!fds->fd_count)
        {
            // ResetEvent(m_write_event);
            check_stop(100);
            return;
        }

        socketio::milliseconds_to_timeval(100, tv);

        const int selres = select(
            static_cast<int>(fds->fd_count),  // "ignored"
            nullptr, fds, nullptr, &tv);

        if (selres == SOCKET_ERROR)
        {
#ifdef _DEBUG
            const auto wsaerror = WSAGetLastError();
            CIX_UNVAR(wsaerror);
            assert(0);
#endif

            // there is still something to write so avoid consuming too much
            // CPU in an infinite loop
            check_stop(100);
            return;
        }
        else if (selres == 0)  // timeout
        {
            return;
        }
        else
        {
            u_int count = 0;
            u_int idx = detail::fdset_rand(fds->fd_count);

            for (; count < fds->fd_count; ++count, ++idx)
            {
                if (idx >= fds->fd_count)
                    idx = 0;

                this->write_thread__do(fds->fd_array[idx]);
            }

            std::scoped_lock lock(m_mutex);

            if (m_write_queue.empty())
            {
                ResetEvent(m_write_event);
                break;
            }
        }
    }
}


void socketio::write_thread__do(SOCKET socket)
{
    cix::lock_guard lock(m_mutex);
    std::list<bytes_t> packets;

    // ensure socket has not been unregistered during the call to select()
    if (!m_fdset_read.has(socket))
        return;

    auto queue_it = m_write_queue.find(socket);
    if (queue_it == m_write_queue.end())
    {
        m_fdset_write.unregister_socket(socket);
        return;
    }

    packets.swap(queue_it->second);
    m_write_queue.erase(queue_it);
    queue_it = m_write_queue.end();

    lock.unlock();

    for (auto packet_it = packets.begin(); packet_it != packets.end(); )
    {
        const auto sent = socketio::send_impl(socket, *packet_it);

        if (sent >= (*packet_it).size())
        {
            packet_it = packets.erase(packet_it);
        }
        else if (sent > 0)
        {
            auto* pp = &(*packet_it);
            pp->erase(pp->begin(), std::next(pp->begin(), sent));
            break;
        }
        else if (sent == 0)
        {
            break;
        }
    }

    lock.lock();

    if (packets.empty())
    {
        m_fdset_write.unregister_socket(socket);
    }
    else
    {
        // re-inject (push front) unsent packets into the queue

        queue_it = m_write_queue.find(socket);

        if (queue_it == m_write_queue.end())
        {
            m_write_queue.insert(std::make_pair(socket, packets));
        }
        else
        {
            std::move(
                queue_it->second.begin(), queue_it->second.end(),
                std::back_inserter(packets));

            queue_it->second.swap(packets);
        }
    }
}


void socketio::unregister_non_sockets(fd_set& fds)
{
    std::vector<SOCKET> to_unreg;

    for (u_int idx = 0; idx < fds.fd_count; ++idx)
    {
        const auto socket = fds.fd_array[idx];

        if (GetFileType(reinterpret_cast<HANDLE>(socket)) != FILE_TYPE_PIPE)
            to_unreg.push_back(socket);
    }

    if (!to_unreg.empty())
    {
        std::scoped_lock lock(m_mutex);

        for (const auto socket : to_unreg)
        {
            this->notify_disconnected(socket);
            this->unregister_socket(socket);
        }
    }
}


void socketio::notify_recv(SOCKET socket, bytes_t&& packet)
{
    cix::lock_guard lock(m_mutex);
    auto listener = m_listener.lock();
    lock.unlock();

    if (listener)
        listener->on_socketio_recv(socket, std::move(packet));
}


void socketio::notify_disconnected(SOCKET socket)
{
    cix::lock_guard lock(m_mutex);
    auto listener = m_listener.lock();
    lock.unlock();

    if (listener)
        listener->on_socketio_disconnected(socket);
}


void socketio::milliseconds_to_timeval(long milliseconds, TIMEVAL& tv)
{
    if (milliseconds <= 0)
    {
        assert(milliseconds == 0);

        tv.tv_sec = 0;
        tv.tv_usec = 0;
    }
    else if (milliseconds < 1000)
    {
        tv.tv_sec = 0;
        tv.tv_usec = milliseconds * 1000;
    }
    else
    {
        tv.tv_sec = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;
    }
}


int socketio::enable_socket_nonblocking_mode(SOCKET socket, bool enable)
{
    u_long nonblocking = enable ? 1 : 0;

    if (SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &nonblocking))
        return WSAGetLastError();

    return 0;
}


std::size_t socketio::send_impl(SOCKET socket, const bytes_t& packet)
{
    if (packet.empty())
    {
        WSASetLastError(0);
        return 0;
    }

    std::size_t sent = 0;

    for (;;)
    {
        const int to_send = static_cast<int>(std::min(
            packet.size() - sent,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));

        const int res = ::send(
            socket,
            reinterpret_cast<const char*>(packet.data() + sent),
            to_send,
            0);

        if (res == SOCKET_ERROR)
        {
#ifdef _DEBUG
            const auto wsaerror = WSAGetLastError();
            CIX_UNVAR(wsaerror);
            assert(0);
#endif
            break;
        }
        else if (res == 0)
        {
            WSASetLastError(0);
            break;
        }
        else if (res > 0)
        {
            sent += static_cast<std::size_t>(res);
            if (sent >= packet.size())
                break;
        }
    }

    return sent;
}
