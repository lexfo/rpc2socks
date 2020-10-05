// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

#if defined(CIX_ENABLE_WIN_NAMEDPIPE_SERVER) && (CIX_PLATFORM == CIX_PLATFORM_WINDOWS)

namespace cix {

std::mutex win_namedpipe_server::ms_overlapped_registry_mutex;
std::map<
    win_namedpipe_server::instance_t::overlapped_t*,
    std::shared_ptr<win_namedpipe_server::instance_t::overlapped_t>>
        win_namedpipe_server::ms_overlapped_registry;


win_namedpipe_server::win_namedpipe_server()
    : m_stop_event{nullptr}
    , m_connect_event{nullptr}
    , m_proceed_event{nullptr}
    , m_flags{flag_default}
{
    m_stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_stop_event)
        CIX_THROW_WINERR("failed to create stop event");

    m_connect_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_connect_event)
        CIX_THROW_WINERR("failed to create conn event");

    m_proceed_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_proceed_event)
        CIX_THROW_WINERR("failed to create write event");
}


win_namedpipe_server::~win_namedpipe_server()
{
    this->stop();

    CloseHandle(m_proceed_event);
    CloseHandle(m_connect_event);
    CloseHandle(m_stop_event);
}


void win_namedpipe_server::set_flags(flags_t flags)
{
    std::scoped_lock lock(m_mutex);
    m_flags = flags;
}


void win_namedpipe_server::set_path(const std::wstring& pipe_path)
{
    std::scoped_lock lock(m_mutex);
    m_path = pipe_path;
    m_path.shrink_to_fit();
}


void win_namedpipe_server::set_listener(std::shared_ptr<listener_t> listener)
{
    std::scoped_lock lock(m_mutex);
    m_listener = listener;
}


void win_namedpipe_server::launch()
{
    std::scoped_lock lock(m_mutex);

    if (WAIT_OBJECT_0 == WaitForSingleObject(m_stop_event, 0))
        return;

    if (m_thread && m_thread->joinable())
        return;

    this->stop();  // so to cleanup internal state

    ResetEvent(m_stop_event);
    ResetEvent(m_connect_event);
    ResetEvent(m_proceed_event);

    m_thread = std::make_unique<std::thread>(
        std::bind(&win_namedpipe_server::maintenance_thread, this));
}


void win_namedpipe_server::stop()
{
    cix::lock_guard lock(m_mutex);

    SetEvent(m_stop_event);

    for (auto instance_pair : m_instances)
        instance_pair.second->disconnect();

    if (m_thread)
    {
        if (m_thread->joinable())
        {
            lock.unlock();
            m_thread->join();
            lock.lock();
        }

        m_thread.reset();
    }

    m_proceed.clear();
    m_instances.clear();

    ResetEvent(m_connect_event);
    ResetEvent(m_proceed_event);
}


bool win_namedpipe_server::send(
    instance_token_t instance_token, bytes_t&& packet)
{
    std::scoped_lock lock(m_mutex);

    auto it = m_instances.find(instance_token);
    if (it == m_instances.end())
        return false;

    auto instance = it->second;

    if (instance->write(std::move(packet)))
    {
        m_proceed.insert(instance_token);
        SetEvent(m_proceed_event);
        return true;
    }
    else
    {
        return false;
    }
}


bool win_namedpipe_server::send_to_first(bytes_t&& packet)
{
    std::scoped_lock lock(m_mutex);

    for (auto& instance_pair : m_instances)
    {
        auto& instance = instance_pair.second;

        // std::move() call is ok here because write() only needs to move the
        // packet if instance is still connected - i.e. return value is true
        if (instance->write(std::move(packet)))
        {
            m_proceed.insert(instance_pair.first);
            SetEvent(m_proceed_event);
            return true;
        }
    }

    return false;
}


std::size_t win_namedpipe_server::broadcast_packet(bytes_t&& packet)
{
    std::scoped_lock lock(m_mutex);
    std::size_t pushed = 0;

    for (auto& instance_pair : m_instances)
    {
        auto& instance = instance_pair.second;
        bytes_t packet_copy(packet.begin(), packet.end());

        if (instance->write(std::move(packet_copy)))
        {
            m_proceed.insert(instance_pair.first);
            ++pushed;
        }
    }

    if (pushed > 0)
        SetEvent(m_proceed_event);

    return pushed;
}


std::size_t win_namedpipe_server::get_output_queue_size(
    instance_token_t instance_token) const
{
    std::scoped_lock lock(m_mutex);

    auto it = m_instances.find(instance_token);
    if (it == m_instances.end())
        return win_namedpipe_server::invalid_queue_size;

    return it->second->output_queue_size();
}


bool win_namedpipe_server::disconnect_instance(instance_token_t instance_token)
{
    cix::lock_guard lock(m_mutex);

    auto it = m_instances.find(instance_token);
    if (it == m_instances.end())
        return false;

    auto instance = it->second;

    lock.unlock();

    if (instance)
    {
        instance->disconnect();
        return true;
    }

    return false;
}


void win_namedpipe_server::maintenance_thread()
{
    const HANDLE events[] = { m_stop_event, m_connect_event, m_proceed_event };
    HANDLE pipe_handle = nullptr;  // handle of the new pipe instance
    OVERLAPPED ol_connect;
    DWORD wait_time = INFINITE;
    bool connecting = false;

    CIX_THREAD_SET_NAME_STATIC(GetCurrentThreadId(), "win_namedpipe_server");

    for (;;)
    {
        if (!pipe_handle)
            pipe_handle = this->open_and_listen(&ol_connect, &connecting);

        // In case open_and_listen() succeeded right away, mimic the behavior of
        // a successful wait for a new connection instance by raising
        // m_connect_event.
        if (pipe_handle && !connecting)
            SetEvent(m_connect_event);

        wait_time = pipe_handle ? INFINITE : 5000;

        // flush pending APCs first
        while (
            !pipe_handle &&
            WAIT_IO_COMPLETION == WaitForSingleObjectEx(m_stop_event, 0, TRUE))
        { ; }

        const auto wait_res = WaitForMultipleObjectsEx(
            static_cast<DWORD>(cix::countof(events)),
            reinterpret_cast<const HANDLE*>(&events),
            FALSE, wait_time, TRUE);  // alertable

        if (wait_res == WAIT_OBJECT_0)  // stop event
        {
            break;
        }
        else if (wait_res == WAIT_OBJECT_0 + 1)  // connect event
        {
            assert(pipe_handle);

            if (pipe_handle && connecting)
            {
                DWORD num_bytes = 0;
                const auto ol_success = GetOverlappedResult(
                    pipe_handle, &ol_connect, &num_bytes, FALSE);
                const auto ol_error = GetLastError();

                if (!ol_success)
                {
                    // LOGERROR(
                    //     "GetOverlappedResult failed on named pipe (error {})",
                    //     ol_error);
                    assert(0);

                    CloseHandle(pipe_handle);
                    pipe_handle = nullptr;
                }
            }

            if (pipe_handle)
                this->create_instance(pipe_handle);

            ResetEvent(m_connect_event);
            pipe_handle = nullptr;
            connecting = false;
        }
        else if (wait_res == WAIT_OBJECT_0 + 2)  // proceed event
        {
            this->handle_proceed_event();
        }
        else if (wait_res == WAIT_IO_COMPLETION)
        {
            continue;
        }
        else
        {
#ifdef _DEBUG
            const auto error = GetLastError();
            // LOGERROR(
            //     "named pipe server worker failed to enter into waiting mode "
            //     "(result {}; error {})",
            //     wait_res, error);
            assert(0);
#endif
            break;
        }
    }
}


HANDLE win_namedpipe_server::open_and_listen(OVERLAPPED* ol, bool* out_connecting)
{
    HANDLE pipe_handle = nullptr;
    std::wstring pipe_path;
    flags_t flags;

    *out_connecting = false;

    // local copy required pipe properties
    {
        std::scoped_lock lock(m_mutex);

        pipe_path = m_path;
        flags = m_flags;
    }

    // TODO: burst test with/without WRITE_THROUGH flag

    const DWORD open_mode =
        PIPE_ACCESS_DUPLEX |  // FILE_FLAG_WRITE_THROUGH |
        FILE_FLAG_OVERLAPPED;

    const DWORD pipe_type =
        PIPE_WAIT |
        ((flags & flag_message) != 0 ?
            (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE) :
            PIPE_TYPE_BYTE) |
        ((flags & flag_accept_remote) != 0 ?
            PIPE_ACCEPT_REMOTE_CLIENTS :
            PIPE_REJECT_REMOTE_CLIENTS);

    SECURITY_ATTRIBUTES sa = {0};
    SECURITY_DESCRIPTOR sd;

    if ((flags & flag_impersonate) != 0)
    {
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, nullptr, TRUE);

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = TRUE;
    }

    pipe_handle = CreateNamedPipeW(
        pipe_path.c_str(),
        open_mode,
        pipe_type,
        PIPE_UNLIMITED_INSTANCES,
        win_namedpipe_server::io_buffer_default_size,  // output
        win_namedpipe_server::io_buffer_default_size,  // input
        INFINITE,  // default timeout
        (flags & flag_impersonate) != 0 ? &sa : nullptr);

    if (pipe_handle == INVALID_HANDLE_VALUE)
    {
#ifdef _DEBUG
        const auto error = GetLastError();
        // LOGERROR("CreateNamedPipe failed (error {})", error);
#endif
        return nullptr;
    }

    // asynchronously wait for a new client
    *ol = {};
    ol->hEvent = m_connect_event;
    ResetEvent(m_connect_event);
    SetLastError(0);
    const auto conn_res = ConnectNamedPipe(pipe_handle, ol);
    const auto conn_error = GetLastError();

    if (conn_res || conn_error == ERROR_PIPE_CONNECTED)
    {
        ResetEvent(m_connect_event);
        return pipe_handle;
    }
    else if (!conn_res && conn_error == ERROR_IO_PENDING)
    {
        *out_connecting = true;
        return pipe_handle;
    }
    else
    {
        CloseHandle(pipe_handle);
        // LOGERROR("ConnectNamedPipe failed (error {})", conn_error);
        assert(0);
        return nullptr;
    }
}


void win_namedpipe_server::create_instance(HANDLE pipe_handle)
{
    cix::lock_guard lock(m_mutex);

    auto self = this->shared_from_this();
    auto instance = std::make_shared<win_namedpipe_server::instance_t>(
        self, pipe_handle);
    const auto token = instance->token();

    m_instances[token] = instance;

    auto listener = m_listener.lock();

    lock.unlock();

    if (listener)
        listener->on_namedpipe_connected(self, token);

    instance->proceed();
}


void win_namedpipe_server::handle_proceed_event()
{
    std::scoped_lock lock(m_mutex);

    decltype(m_proceed) proceed_instances;

    // assert(!m_proceed.empty());
    m_proceed.swap(proceed_instances);
    ResetEvent(m_proceed_event);

    for (const auto& instance_token : proceed_instances)
    {
        auto it = m_instances.find(instance_token);
        if (it == m_instances.end())
            continue;

        auto instance = it->second;
        if (!instance)
        {
            assert(0);
            continue;
        }

        instance->proceed();
    }
}


void win_namedpipe_server::notify_read(instance_token_t token, bytes_t&& packet)
{
    std::shared_ptr<listener_t> listener;

    {
        std::scoped_lock lock(m_mutex);
        listener = m_listener.lock();
    }

    if (listener)
    {
        listener->on_namedpipe_recv(
            this->shared_from_this(), token, std::move(packet));
    }
}


void win_namedpipe_server::notify_written(
    instance_token_t token, bytes_t&& packet, std::size_t output_queue_size)
{
    std::shared_ptr<listener_t> listener;

    {
        std::scoped_lock lock(m_mutex);
        listener = m_listener.lock();
    }

    if (listener)
    {
        listener->on_namedpipe_sent(
            this->shared_from_this(), token, std::move(packet),
            output_queue_size);
    }
}


void win_namedpipe_server::notify_closed(instance_token_t token)
{
    std::shared_ptr<listener_t> listener;

    {
        std::scoped_lock lock(m_mutex);

        listener = m_listener.lock();

        m_proceed.erase(token);
        m_instances.erase(token);
    }

    if (listener)
        listener->on_namedpipe_closed(this->shared_from_this(), token);
}


void WINAPI win_namedpipe_server::apc_completed_read(
    DWORD error, DWORD bytes_read, LPOVERLAPPED ol_)
{
    cix::lock_guard lock(win_namedpipe_server::ms_overlapped_registry_mutex);

    auto it = ms_overlapped_registry.find(
        reinterpret_cast<instance_t::overlapped_t*>(ol_));

    if (it == ms_overlapped_registry.end())
    {
        // LOGWARNING(
        //     "named pipe's read::overlapped_t not found in registry "
        //     "(error {}; read {} bytes)",
        //     error, bytes_read);
        assert(0);
        return;
    }

    auto ol = it->second;

    ms_overlapped_registry.erase(it);
    it = ms_overlapped_registry.end();

    lock.unlock();

    auto instance = ol->instance.lock();
    // assert(instance);

    if (instance)
    {
        assert(
            error == 0 ||
            error == ERROR_BROKEN_PIPE ||
            error == ERROR_PIPE_NOT_CONNECTED);

        if (error == 0 && bytes_read > 0)
        {
            ol->packet.resize(bytes_read);
            instance->on_read(ol);
        }
        else
        {
            ol.reset();
            instance->close();
        }
    }
}


void WINAPI win_namedpipe_server::apc_completed_write(
    DWORD error, DWORD bytes_written, LPOVERLAPPED ol_)
{
    cix::lock_guard lock(win_namedpipe_server::ms_overlapped_registry_mutex);

    CIX_UNVAR(bytes_written);

    auto it = ms_overlapped_registry.find(
        reinterpret_cast<instance_t::overlapped_t*>(ol_));

    if (it == ms_overlapped_registry.end())
    {
        // LOGWARNING(
        //     "named pipe's write::overlapped_t not found in registry "
        //     "(error {}; written {} bytes)",
        //     error, bytes_written);
        assert(0);
        return;
    }

    auto ol = it->second;

    ms_overlapped_registry.erase(it);
    it = ms_overlapped_registry.end();

    lock.unlock();

    auto instance = ol->instance.lock();
    // assert(instance);

    if (instance)
    {
        if (error == 0)
        {
            instance->on_written(ol);
            ol.reset();
        }
        else
        {
            assert(
                error == ERROR_BROKEN_PIPE ||
                error == ERROR_PIPE_NOT_CONNECTED);

            ol.reset();
            instance->close();
        }
    }
}



//******************************************************************************



win_namedpipe_server::instance_t::instance_t(
    std::shared_ptr<win_namedpipe_server> parent,
    HANDLE pipe)
: m_parent(parent)
, m_token{cix::bit_cast<instance_token_t>(pipe)}
, m_pipe{pipe}
{
    // note: even though m_token and m_pipe values are equal, m_pipe may be
    // closed and reset, where as token's lifetime is bound to instance_t

    assert(parent);
    assert(m_token);
    assert(m_pipe);
}


win_namedpipe_server::instance_t::~instance_t()
{
    this->close();
}


void win_namedpipe_server::instance_t::disconnect()
{
    std::scoped_lock lock(m_mutex);

    // * disconnect() must be called instead of close() when calling thread is
    //   not the maintenance thread
    // * this call to DisconnectNamedPipe() will make the maintenance thread to
    //   close() this instance gracefully
    if (m_pipe)
        DisconnectNamedPipe(m_pipe);
}


void win_namedpipe_server::instance_t::close()
{
    cix::lock_guard lock(m_mutex);
    auto parent = m_parent.lock();
    bool notify_closed = false;

    if (m_pipe)
    {
        notify_closed = true;

#if WINVER >= 0x0600  // Vista
        CancelIoEx(m_pipe, nullptr);
#else
        CancelIo(m_pipe);
#endif
        DisconnectNamedPipe(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = nullptr;
    }

    m_olread.reset();
    m_olwrites.clear();

    // clear() output
    decltype(m_output) empty_queue;
    m_output.swap(empty_queue);

    lock.unlock();

    if (notify_closed && parent)
        parent->notify_closed(m_token);
}


win_namedpipe_server::instance_token_t
win_namedpipe_server::instance_t::token() const
{
    std::scoped_lock lock(m_mutex);
    return m_token;
}


std::shared_ptr<win_namedpipe_server>
win_namedpipe_server::instance_t::parent() const
{
    std::scoped_lock lock(m_mutex);
    return m_parent.lock();
}


bool win_namedpipe_server::instance_t::orphan() const
{
    std::scoped_lock lock(m_mutex);
    return m_parent.expired();
}


std::size_t win_namedpipe_server::instance_t::output_queue_size() const
{
    std::scoped_lock lock(m_mutex);
    return m_output.size();
}


void win_namedpipe_server::instance_t::proceed()
{
    cix::lock_guard lock(m_mutex);
    cix::lock_guard ol_lock(
        win_namedpipe_server::ms_overlapped_registry_mutex,
        std::defer_lock);  // do not lock yet

    // CAUTION: proceed() must be called from win_namedpipe_server's maintenance
    // thread only so that IO completion routines can be handled by it. This is
    // because a thread must be in waiting and alertable state for a completion
    // routine to be executed by the kernel.
#ifdef _DEBUG
    {
        auto parent = m_parent.lock();
        assert(parent &&
            parent->m_thread &&
            parent->m_thread->get_id() == std::this_thread::get_id());
    }
#endif

    if (!m_pipe)
        return;

    // cleanup m_olwrites
    if (!m_output.empty() && !m_olwrites.empty())
    {
        // C++20
        // std::erase_if(
        //     m_olwrites,
        //     [] (const auto& item) { return item->second.expired(); });
        for (auto it = m_olwrites.begin(); it != m_olwrites.end(); )
        {
            if (it->second.expired())
                it = m_olwrites.erase(it);
            else
                ++it;
        }
    }

    if (!m_output.empty() &&
        (win_namedpipe_server::max_pending_kernel_writes == 0 ||
            m_olwrites.size() < win_namedpipe_server::max_pending_kernel_writes))
    {
        // create overlapped_t object
        auto wol = std::make_shared<overlapped_t>(
            this->shared_from_this(),
            m_output.front());  // CAUTION: front() gets swap()'ed
        // wol->ol.Offset = 0xffffffff;
        // wol->ol.OffsetHigh = 0xffffffff;

        m_olwrites[wol.get()] = wol;

        // register overlapped_t object
        ol_lock.lock();
        ms_overlapped_registry[wol.get()] = wol;
        ol_lock.unlock();

        // start writing
        const auto write_success = WriteFileEx(
            m_pipe, wol->packet.data(),
            static_cast<DWORD>(wol->packet.size()),
            reinterpret_cast<OVERLAPPED*>(wol.get()),
            win_namedpipe_server::apc_completed_write);

        if (!write_success)
        {
            const auto write_error = GetLastError();

            ol_lock.lock();
            ms_overlapped_registry.erase(wol.get());
            ol_lock.unlock();

            m_output.front().swap(wol->packet);  // swap back
            m_olwrites.erase(wol.get());
            wol.reset();

            if (write_error != ERROR_INVALID_USER_BUFFER &&
                write_error != ERROR_NOT_ENOUGH_MEMORY)
            {
                // LOGERROR(
                //     "WriteFileEx on named pipe failed ({} bytes; error {})",
                //     m_output.front().size(), write_error);
                // assert(0);

                lock.unlock();
                this->close();
                return;
            }
            else
            {
#ifdef CIX_DEBUG
                // TEST
                OutputDebugStringW(cix::string::fmt(
                    L"namedpipe write error: {}\n", write_error).c_str());
                // assert(0);
                // TESTEND
#endif
            }
        }
        else
        {
            m_output.pop();
        }
    }

    if (!m_olread)
    {
        // create overlapped_t object
        m_olread = std::make_shared<overlapped_t>(
            this->shared_from_this(),
            bytes_t(win_namedpipe_server::io_buffer_default_size, 0));

        // register overlapped_t object
        ol_lock.lock();
        ms_overlapped_registry[m_olread.get()] = m_olread;
        ol_lock.unlock();

        // start reading
        const auto read_success = ReadFileEx(
            m_pipe, m_olread->packet.data(),
            static_cast<DWORD>(m_olread->packet.size()),
            reinterpret_cast<OVERLAPPED*>(m_olread.get()),
            win_namedpipe_server::apc_completed_read);

        if (!read_success)
        {
            const auto read_error = GetLastError();

            if (read_error != ERROR_INVALID_USER_BUFFER &&
                read_error != ERROR_NOT_ENOUGH_MEMORY)
            {
                // if (read_error != ERROR_BROKEN_PIPE)  // pipe has been closed
                // {
                //     LOGERROR(
                //         "ReadFileEx on named pipe failed (error {})",
                //         read_error);
                // }

                ol_lock.lock();
                ms_overlapped_registry.erase(m_olread.get());
                ol_lock.unlock();

                m_olread.reset();

                lock.unlock();
                this->close();
                return;
            }
            else
            {
#ifdef CIX_DEBUG
                // TEST
                OutputDebugStringW(cix::string::fmt(
                    L"namedpipe read error: {}\n", read_error).c_str());
                // assert(0);
                // TESTEND
#endif
            }
        }
    }
}


bool win_namedpipe_server::instance_t::write(bytes_t&& packet)
{
    std::scoped_lock lock(m_mutex);

    if (!m_pipe)
        return false;

    // if (m_output.empty() || (m_flags & flag_message) != 0)
    // {
    //     m_output.push(std::move(packet));
    // }
    // else
    // {
    //     auto& back = m_output.back();
    //     back.reserve(back.size() + packet.size());
    //     std::copy(packet.begin(), packet.end(), std::back_inserter(back));
    // }

    m_output.push(std::move(packet));

    // CAUTION: do not call proceed() from here! see implementation for more
    // details
    // this->proceed();

    return true;
}


void win_namedpipe_server::instance_t::on_read(std::shared_ptr<overlapped_t> ol)
{
    cix::lock_guard lock(m_mutex);

    auto parent = m_parent.lock();

    m_olread.reset();

    lock.unlock();

    if (ol->packet.empty())
    {
        ol.reset();
        this->close();
    }
    else
    {
        if (parent)
            parent->notify_read(m_token, std::move(ol->packet));

        ol.reset();

        this->proceed();
    }
}


void win_namedpipe_server::instance_t::on_written(std::shared_ptr<overlapped_t> ol)
{
    cix::lock_guard lock(m_mutex);

    auto parent = m_parent.lock();

    m_olwrites.erase(ol.get());
    const auto output_queue_size = parent ? m_output.size() : 0;

    lock.unlock();

    if (parent)
    {
        parent->notify_written(
            m_token, std::move(ol->packet), output_queue_size);
    }

    this->proceed();
}

}  // namespace cix

#endif  // #if defined(CIX_ENABLE_WIN_NAMEDPIPE_SERVER) && (CIX_PLATFORM == CIX_PLATFORM_WINDOWS)
