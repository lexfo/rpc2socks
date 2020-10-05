// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


socks_proxy::socks_proxy()
    : m_stop_event{nullptr}
    , m_request_event{nullptr}
{
    m_stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_stop_event)
        CIX_THROW_WINERR("failed to create (S) stop event");

    m_request_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_request_event)
        CIX_THROW_WINERR("failed to create requests event");
}


socks_proxy::~socks_proxy()
{
    this->stop();

    CloseHandle(m_request_event);
    CloseHandle(m_stop_event);
}


void socks_proxy::set_listener(std::shared_ptr<listener_t> listener)
{
    std::scoped_lock lock(m_mutex);
    m_listener = listener;
}


void socks_proxy::stop()
{
    cix::lock_guard lock(m_mutex);

    SetEvent(m_stop_event);

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

    if (m_socketio)
    {
        m_socketio->set_listener(nullptr);
        m_socketio->join();
        m_socketio.reset();
    }
}


void socks_proxy::launch()
{
    std::scoped_lock lock(m_mutex);

    if (WAIT_TIMEOUT != WaitForSingleObject(m_stop_event, 0))
        return;

    if (m_thread && m_thread->joinable())
        return;

    this->stop();  // cleanup internal state

    ResetEvent(m_stop_event);

    if (!m_socketio)
    {
        m_socketio = std::make_shared<socketio>();
        m_socketio->set_stop_event(m_stop_event);
        m_socketio->set_listener(this->shared_from_this());
        m_socketio->launch();
    }

    m_thread = std::make_unique<std::thread>(
        std::bind(&socks_proxy::maintenance_thread, this));
}


socks_proxy::token_t socks_proxy::create_client()
{
    const auto now = cix::ticks_now();
    token_t client_token;

    std::scoped_lock lock(m_mutex);

    // paranoid check
    if (m_clients.size() == m_clients.max_size() ||
        m_clients.size() >= std::numeric_limits<token_t>::max() - 2)
    {
        return invalid_token;
    }

    do
    {
        client_token = m_rand.next64();
    }
    while (
        client_token == 0 ||
        client_token == std::numeric_limits<token_t>::max() ||
        m_clients.find(client_token) != m_clients.end());

    auto client = std::make_shared<client_t>();
    client->token = client_token;
    client->socks_state = socks_state_newclient;
    client->conn = INVALID_SOCKET;
    client->last_activity = now;

    m_clients[client_token] = client;

    return client_token;
}


void socks_proxy::push_request(token_t client_token, bytes_t&& packet)
{
    auto request = std::make_shared<socks_packet_t>(
        client_token, std::move(packet));

    std::scoped_lock lock(m_mutex);

    m_pending_requests.push_back(request);
    SetEvent(m_request_event);
}


void socks_proxy::disconnect_client(token_t client_token)
{
    this->erase_client(client_token);

    // CAUTION:
    // * this could create a deadlock on caller's side, due to the
    //   on_socks_disconnected() callback being called from the same thread
    // * also, since this method is public and there is only one listener, the
    //   owner of this socks_proxy object is already aware about this particular
    //   client being disconnected, if any
    //
    // this->notify_disconnected(client_token);
}


void socks_proxy::maintenance_thread()
{
    const HANDLE events[] = { m_stop_event, m_request_event };

    CIX_THREAD_SET_NAME_STATIC(GetCurrentThreadId(), "socks_proxy");

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
        else if (wait_res == WAIT_OBJECT_0 + 1)  // request event
        {
            this->handle_requests();
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


void socks_proxy::handle_requests()
{
    decltype(m_pending_requests) requests;
    std::shared_ptr<client_t> client;

    cix::lock_guard lock(m_mutex);

    m_pending_requests.swap(requests);
    ResetEvent(m_request_event);

    lock.unlock();

    // IMPORTANT: m_mutex must be unlocked here

    while (!requests.empty())
    {
        auto& request = requests.front();

        lock.lock();

        // find client object
        auto client_it = m_clients.find(request->client_token);
        client =
            client_it != m_clients.end() ?
            client_it->second :
            nullptr;
        client_it = m_clients.end();

        lock.unlock();

        if (client)
        {
            this->handle_socks_request(client, *request);
            client.reset();
        }

        requests.pop_front();
    }
}


void socks_proxy::handle_socks_request(
    std::shared_ptr<client_t> client,
    socks_packet_t& request)
{
    const auto client_token = client->token;
    const auto socks_state = client->socks_state;

    switch (socks_state)
    {
        case socks_state_newclient:
            if (!handle_socks_request__newclient(*client, request))
                goto __close_and_erase_client;
            break;

        case socks_state_needauth:
            if (!handle_socks_request__needauth(*client, request))
                goto __close_and_erase_client;
            break;

        case socks_state_needcmd:
            if (!handle_socks_request__needcmd(*client, request))
                goto __close_and_erase_client;
            break;

        case socks_state_connected:
            if (!handle_socks_request__connected(*client, request))
                goto __close_and_erase_client;
            break;

        default:
            LOGDEBUG("unhandled SOCKS state #{}", socks_state);
            assert(0);
            goto __close_and_erase_client;
    }

    return;

__close_and_erase_client:
    this->request_close(client_token);
    this->erase_client(client_token);
}


bool socks_proxy::handle_socks_request__newclient(
    client_t& client,
    const socks_packet_t& request)
{
    const auto& packet = request.packet;

    // favor no_auth method, support user+pass method
    if (packet.size() >= 3 && packet[0] == 5)  // SOCKS5 only
    {
        const auto nauth = packet[1];
        bool user_pass_auth_supported = false;

        for (std::uint8_t idx = 0; idx < nauth; ++idx)
        {
            const auto auth_type = packet[2 + idx];

            if (auth_type == socks_noauth)  // no auth
            {
                client.socks_state = socks_state_needcmd;
                this->send_to_client(client, bytes_t{5, socks_noauth});
                return true;
            }
            else if (auth_type == socks_auth_userpass)  // user+pass auth
            {
                user_pass_auth_supported = true;
            }
        }

        if (user_pass_auth_supported)
        {
            // go for user+pass auth
            client.socks_state = socks_state_needauth;
            this->send_to_client(client, bytes_t{5, socks_auth_userpass});
            return true;
        }
    }

    // no acceptable auth method found
    this->send_to_client(client, bytes_t{5, socks_auth_no_supported_method});
    return false;
}


bool socks_proxy::handle_socks_request__needauth(
    client_t& client,
    const socks_packet_t& request)
{
    // CAUTION: this assumes socks_auth_userpass auth scheme

    const auto& packet = request.packet;

    if (packet[0] == 1 && packet[1] >= 1 && packet.size() >= 5)
    {
        const auto user_len = static_cast<std::size_t>(packet[1]);

        if (packet.size() >= 4 + user_len)
        {
            const auto pass_len =
                static_cast<std::size_t>(packet[2 + user_len]);

            if (packet.size() == 3 + user_len + pass_len)
            {
                const std::string_view user(
                    reinterpret_cast<const char*>(packet.data()) + 2,
                    user_len);

                const std::string_view pass(
                    reinterpret_cast<const char*>(packet.data()) +
                        2 + user_len + 1,
                    pass_len);

                LOGDEBUG("SOCKS: user [{}], pass [{}]", user, pass);
                CIX_UNVAR(user);
                CIX_UNVAR(pass);

                // TODO: check username and password if needed

                // auth successful
                client.socks_state = socks_state_needcmd;
                this->send_to_client(client, bytes_t{1, 0});
                return true;
            }

            assert(0);
        }

        assert(0);
    }

    // auth failed, or auth packet is malformed
    this->send_to_client(client, bytes_t{1, 1});
    return false;
}


bool socks_proxy::handle_socks_request__needcmd(
    client_t& client,
    const socks_packet_t& request)
{
    const auto& packet = request.packet;
    socks_addr_t addr_type = socks_addr_ipv4;  // default to IPv4
    std::size_t required_min_len = 10;  // 10 bytes for IPv4 CONNECT command
    int addr_family = AF_INET;
    char addr_str[256];
    unsigned short remote_port;
    struct addrinfo* ai_remote = nullptr;
    socks_reply_code_t reply_code = socks_reply_general_failure;

    if (packet.size() < 10 ||
        packet[0] != 5 ||  // SOCKS5 only
        packet[2] != 0)    // reserved value, expected to be null in SOCKS5
    {
        assert(0);
        reply_code = socks_reply_general_failure;
        goto __send_status;
    }

    // only CONNECT method supported
    if (packet[1] != socks_cmd_connect)
    {
        assert(0);
        reply_code = socks_reply_command_not_supported;
        goto __send_status;
    }

    addr_type = static_cast<socks_addr_t>(packet[3]);
    addr_str[0] = '\0';

    switch (addr_type)
    {
        case socks_addr_ipv6:
            required_min_len = 22;
            addr_family = AF_INET6;
            // fall through

        case socks_addr_ipv4:
            if (packet.size() >= required_min_len)
            {
                assert(0);
                reply_code = socks_reply_general_failure;
                goto __send_status;
            }

            if (!wincompat::inet_ntop(
                addr_family, packet.data() + 4,
                reinterpret_cast<char*>(&addr_str), sizeof(addr_str)))
            {
                assert(0);
                reply_code = socks_reply_general_failure;
                goto __send_status;
            }
            break;

        case socks_addr_name:
        {
            const auto name_len = static_cast<std::size_t>(packet[4]);

            required_min_len = 7 + name_len;
            if (required_min_len <= 7 || packet.size() < required_min_len)
            {
                assert(0);
                reply_code = socks_reply_general_failure;
                goto __send_status;
            }

            memcpy_s(&addr_str, sizeof(addr_str), packet.data() + 5, name_len);
            addr_str[name_len] = '\0';
            break;
        }

        default:
            assert(0);
            reply_code = socks_reply_addr_type_not_supported;
            goto __send_status;
    }

    if (!addr_str[0])
    {
        reply_code = socks_reply_general_failure;
        goto __send_status;
    }

    remote_port =
        static_cast<unsigned short>(packet[required_min_len - 2] << 8) |
        static_cast<unsigned short>(packet[required_min_len - 1]);

    if (0 != socks_proxy::resolve(
        reinterpret_cast<const char*>(&addr_str), remote_port, &ai_remote))
    {
        assert(0);
        reply_code = socks_reply_general_failure;
        goto __send_status;
    }

    reply_code = socks_proxy::connect_socket(client, ai_remote);
    if (reply_code != socks_reply_success)
        goto __send_status;

    assert(reply_code == socks_reply_success);

    {
        std::scoped_lock lock(m_mutex);

        if (m_socketio)
            m_socketio->register_socket(client.conn);
    }


__send_status:
    if (reply_code == socks_reply_success)
        client.socks_state = socks_state_connected;

    if (ai_remote)
    {
        freeaddrinfo(ai_remote);
        ai_remote = nullptr;
    }

    if (reply_code != socks_reply_success && client.conn != INVALID_SOCKET)
    {
        closesocket(client.conn);
        client.conn = INVALID_SOCKET;
    }

    this->send_reply_to_client(client, reply_code, addr_type);

    return reply_code == socks_reply_success;
}


bool socks_proxy::handle_socks_request__connected(
    const client_t& client,
    socks_packet_t& request)
{
    if (client.conn == INVALID_SOCKET)
        return false;

    return this->send_to_target(client, std::move(request.packet));
}


void socks_proxy::send_to_client(client_t& client, bytes_t&& packet)
{
    auto response = std::make_shared<socks_packet_t>(
        client.token, std::move(packet));

    this->notify_response(response);
}


void socks_proxy::send_reply_to_client(
    client_t& client, socks_reply_code_t code, socks_addr_t addr_type)
{
    // FIXME: this method does not comply to RFC1928 because it sticks to IPv4
    // address type with null address and port, regardless of the passed
    // *addr_type*

#if 0
    bytes_t packet(22, 0);

    packet[0] = 5;
    packet[1] = code;
    // packet[2] = 0;
    packet[3] = addr_type;

    switch (addr_type)
    {
        default:
            assert(0);
            // fall through

        case socks_addr_name:
            // TODO / FIXME: support this case
            // TODO / FIXME: fill-in ADDR and PORT fields
            addr_type = socks_addr_ipv4;
            packet[3] = socks_addr_ipv4;
            // fall through

        case socks_addr_ipv4:
            packet.resize(10);
            // TODO / FIXME: fill-in ADDR and PORT fields
            break;

        case socks_addr_ipv6:
            packet.resize(22);
            // TODO / FIXME: fill-in ADDR and PORT fields
            break;
    }
#else
    CIX_UNVAR(addr_type);

    bytes_t packet{
        5, code, 0, socks_addr_ipv4,
        0, 0, 0, 0,  // ipv4 addr
        0, 0};       // port
#endif

    auto response = std::make_shared<socks_packet_t>(
        client.token, std::move(packet));

    this->notify_response(response);
}


bool socks_proxy::send_to_target(const client_t& client, bytes_t&& packet)
{
    std::scoped_lock lock(m_mutex);

    if (m_socketio)
        return m_socketio->send(client.conn, std::move(packet));

    return false;
}


std::shared_ptr<socks_proxy::client_t>
socks_proxy::find_client(SOCKET socket) const
{
    std::scoped_lock lock(m_mutex);

    auto it = std::find_if(
        m_clients.cbegin(), m_clients.cend(),
        [&socket](const auto& pair) { return pair.second->conn == socket; });

    return it == m_clients.end() ? nullptr : it->second;
}


void socks_proxy::erase_client(token_t client_token)
{
    std::scoped_lock lock(m_mutex);

    auto client_it = m_clients.find(client_token);
    if (client_it != m_clients.end())
    {
        const auto conn = client_it->second->conn;

        if (conn != INVALID_SOCKET)
        {
            this->disconnect_socket(conn);
            client_it->second->conn = INVALID_SOCKET;
        }

        m_clients.erase(client_it);
    }

    for (auto request_it = m_pending_requests.begin();
        request_it != m_pending_requests.end(); )
    {
        if ((*request_it)->client_token == client_token)
            request_it = m_pending_requests.erase(request_it);
        else
            ++request_it;
    }
}


void socks_proxy::disconnect_socket(SOCKET socket)
{
    if (socket != INVALID_SOCKET)
    {
        // CAUTION: keep this code lock-free so that we avoid deadlock due to
        // the on_socketio_disconnected() callback (i.e. it acquires m_mutex)
        auto sockio = m_socketio;
        if (sockio)
            sockio->disconnect_and_unregister_socket(socket);
    }
}


void socks_proxy::notify_response(std::shared_ptr<socks_packet_t> response)
{
    cix::lock_guard lock(m_mutex);
    auto listener = m_listener.lock();
    lock.unlock();

    if (listener)
        listener->on_socks_response(this->shared_from_this(), response);
}


void socks_proxy::request_close(token_t client_token)
{
    cix::lock_guard lock(m_mutex);
    auto listener = m_listener.lock();
    lock.unlock();

    if (listener)
    {
        listener->on_socks_close_client(
            this->shared_from_this(), client_token);
    }
}


void socks_proxy::notify_disconnected(token_t client_token)
{
    cix::lock_guard lock(m_mutex);
    auto listener = m_listener.lock();
    lock.unlock();

    if (listener)
    {
        listener->on_socks_disconnected(
            this->shared_from_this(), client_token);
    }
}


void socks_proxy::on_socketio_recv(SOCKET socket, bytes_t&& packet)
{
    cix::lock_guard lock(m_mutex);
    auto client = this->find_client(socket);
    lock.unlock();

    if (client)
    {
        this->send_to_client(*client, std::move(packet));
    }
    else
    {
        // proxy client disconnected, so disconnect from SOCKS target
        lock.lock();
        this->disconnect_socket(socket);
    }
}


void socks_proxy::on_socketio_disconnected(SOCKET socket)
{
    cix::lock_guard lock(m_mutex);
    auto client = this->find_client(socket);
    lock.unlock();

    if (client)
    {
        const auto client_token = client->token;
        client.reset();
        this->disconnect_client(client_token);
    }
}


socks_proxy::socks_reply_code_t
socks_proxy::wsaerror_to_socks_reply(int wsaerror)
{
    switch (wsaerror)
    {
        case WSAENETDOWN:
        case WSAENETUNREACH:
            return socks_reply_net_unreachable;

        case WSAEHOSTUNREACH:
            return socks_reply_host_unreachable;

        case WSAECONNREFUSED:
            return socks_reply_conn_refused;

        case WSAEAFNOSUPPORT:
        case WSAEPROTONOSUPPORT:
        case WSAEPROTOTYPE:
        case WSAESOCKTNOSUPPORT:
            return socks_reply_addr_type_not_supported;

        case WSAETIMEDOUT:
            return socks_reply_ttl_expired;

        default:
            return socks_reply_general_failure;
    }
}


int socks_proxy::resolve(
    const char* host, unsigned short port, struct addrinfo** out_addr,
    int ai_family, int ai_flags)
{
    struct addrinfo hints;
    char port_str[8];

    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = ai_flags;

    snprintf(port_str, sizeof(port_str), "%u", port);

    return getaddrinfo(host, port_str, &hints, out_addr);
}


socks_proxy::socks_reply_code_t
socks_proxy::connect_socket(
    client_t& client, struct addrinfo* remote_addr)
{
    socks_reply_code_t status = socks_reply_success;
    int wsaerror;

    assert(client.conn == INVALID_SOCKET);

    for (struct addrinfo* ai = remote_addr; ai; ai = ai->ai_next)
    {
        // CAUTION: we expect SOCK_STREAM and IPPROTO_TCP anyway so do not use
        // values from ai for those

        // client.conn = WSASocket(
        //     ai->ai_addr->sa_family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
        //     WSA_FLAG_OVERLAPPED);
        client.conn = socket(ai->ai_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
        if (client.conn == INVALID_SOCKET)
        {
            wsaerror = WSAGetLastError();

            LOGDEBUG(
                "failed to create socket to SOCKS target (error {})",
                wsaerror);

            if (status == socks_reply_success)
                status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

            continue;
        }

        // setup socket
        {
            for (const int sockopt : { SO_RCVTIMEO, SO_SNDTIMEO })
            {
                DWORD timeout = socks_proxy::socket_io_timeout;

                if (SOCKET_ERROR == setsockopt(
                    client.conn, SOL_SOCKET, sockopt,
                    reinterpret_cast<const char*>(&timeout), sizeof(DWORD)))
                {
                    wsaerror = WSAGetLastError();

                    closesocket(client.conn);
                    client.conn = INVALID_SOCKET;

                    LOGDEBUG(
                        "failed to set SOCKS socket's recv/send timeout "
                        "(error {}; sockopt {})",
                        wsaerror, sockopt);

                    if (status == socks_reply_success)
                        status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                    return status;
                }
            }

            // appy non-blocking mode so that we can connect() with a timeout
            wsaerror = socketio::enable_socket_nonblocking_mode(
                client.conn, true);
            if (wsaerror != 0)
            {
                closesocket(client.conn);
                client.conn = INVALID_SOCKET;

                LOGDEBUG(
                    "failed to set SOCKS socket in non-blocking mode (error {})",
                    wsaerror);

                if (status == socks_reply_success)
                    status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                return status;
            }
        }

        // connect
        // "With a nonblocking socket, the connection attempt cannot be
        // completed immediately. In this case, connect will return
        // SOCKET_ERROR, and WSAGetLastError will return WSAEWOULDBLOCK." - msdn
        if (SOCKET_ERROR == connect(
            client.conn, ai->ai_addr, static_cast<int>(ai->ai_addrlen)))
        {
            wsaerror = WSAGetLastError();

            if (wsaerror != WSAEWOULDBLOCK)
            {
                LOGDEBUG(
                    "failed to connect to SOCKS target (error {})",
                    wsaerror);

                closesocket(client.conn);
                client.conn = INVALID_SOCKET;

                if (status == socks_reply_success)
                    status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                continue;
            }
            else
            {
                fd_set fds_write;
                fd_set fds_except;
                TIMEVAL tv;
                int selres;

                FD_ZERO(&fds_write);
                FD_ZERO(&fds_except);
                FD_SET(client.conn, &fds_write);
                FD_SET(client.conn, &fds_except);

                socketio::milliseconds_to_timeval(
                    static_cast<long>(socks_proxy::socket_connect_timeout),
                    tv);

                selres = select(0, nullptr, &fds_write, &fds_except, &tv);
                if (selres == SOCKET_ERROR || selres == 0)
                {
                    if (selres == 0)
                    {
                        wsaerror = WSAETIMEDOUT;
                        WSASetLastError(wsaerror);
                    }

                    LOGDEBUG(
                        "failed to connect to SOCKS target (error {})",
                        wsaerror);

                    closesocket(client.conn);
                    client.conn = INVALID_SOCKET;

                    if (status == socks_reply_success)
                        status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                    continue;
                }
                else if (FD_ISSET(client.conn, &fds_except))
                {
                    int optlen = sizeof(wsaerror);

                    getsockopt(
                        client.conn, SOL_SOCKET, SO_ERROR,
                        reinterpret_cast<char*>(&wsaerror), &optlen);

                    closesocket(client.conn);
                    client.conn = INVALID_SOCKET;

                    WSASetLastError(wsaerror);

                    if (status == socks_reply_success)
                        status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                    continue;
                }

                // connect succeeded, set socket back to blocking mode
                wsaerror = socketio::enable_socket_nonblocking_mode(
                    client.conn, false);
                if (wsaerror != 0)
                {
                    closesocket(client.conn);
                    client.conn = INVALID_SOCKET;

                    LOGDEBUG(
                        "failed to set SOCKS socket back to blocking mode "
                        "(error {})",
                        wsaerror);

                    if (status == socks_reply_success)
                        status = socks_proxy::wsaerror_to_socks_reply(wsaerror);

                    continue;
                }

                return socks_reply_success;
            }
        }
    }

    assert(client.conn == INVALID_SOCKET);

    if (status == socks_reply_success)
        status = socks_reply_general_failure;

    return status;
}
