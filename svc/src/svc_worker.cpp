// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


svc_worker::svc_worker()
    : m_stop_event{nullptr}
    , m_recv_event{nullptr}
    , m_pipe(std::make_shared<cix::win_namedpipe_server>())
    , m_socks_proxy(std::make_shared<socks_proxy>())
{
    m_recv_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // manual reset
    if (!m_recv_event)
        CIX_THROW_WINERR("failed to create pipe event");
}


svc_worker::~svc_worker()
{
    m_socks_proxy->set_listener(nullptr);
    m_pipe->set_listener(nullptr);

    m_socks_proxy->stop();
    m_pipe->stop();

    m_socks_proxy.reset();
    m_pipe.reset();

    CloseHandle(m_recv_event);

    WSACleanup();
}


exit_t svc_worker::init(HANDLE stop_event, const std::wstring& pipe_base_name_)
{
    assert(stop_event);
    m_stop_event = stop_event;

    m_pipe_path = L"\\\\.\\pipe\\";

    if (!pipe_base_name_.empty())
    {
        m_pipe_path += pipe_base_name_;
    }
    else
    {
        std::wstring mod_path;
        std::wstring svc_name;

        if (!svc::auto_name(mod_path, svc_name) || svc_name.empty())
            return APP_EXITCODE_ERROR;

        m_pipe_path += svc_name;
    }

    {
        WSADATA wsadata{};
        const int ires = WSAStartup(MAKEWORD(2, 2), &wsadata);

        if (ires != 0)
        {
            LOGERROR("WSAStartup failed (error {})", ires);
            return APP_EXITCODE_API;
        }
    }

    return APP_EXITCODE_OK;
}


exit_t svc_worker::main_loop()
{
    assert(m_stop_event);
    assert(m_recv_event);

    const HANDLE events[] = { m_stop_event, m_recv_event };

    m_socks_proxy->set_listener(this->shared_from_this());

    m_pipe->set_flags(
        cix::win_namedpipe_server::flag_accept_remote |
        cix::win_namedpipe_server::flag_impersonate);
    m_pipe->set_path(m_pipe_path);
    m_pipe->set_listener(this->shared_from_this());

    m_socks_proxy->launch();
    m_pipe->launch();

    for (;;)
    {
        const auto wait_res = WaitForMultipleObjects(
            static_cast<DWORD>(cix::countof(events)),
            reinterpret_cast<const HANDLE*>(&events),
            FALSE, INFINITE);

        if (wait_res == WAIT_OBJECT_0 + 0)  // stop event
        {
            break;
        }
        else if (wait_res == WAIT_OBJECT_0 + 1)  // received data event
        {
            this->process_received_data();
        }
        // else if (wait_res == WAIT_TIMEOUT)
        // {
        //     // TODO: disconnect expired non-setup channels
        // }
        else
        {
            LOGERROR(
                "worker failed to enter in waiting mode (result {}; error {})",
                wait_res, GetLastError());
            return APP_EXITCODE_API;
        }
    }

    // close pipe and its instances
    this->disconnect_all();
    m_pipe->set_listener(nullptr);
    m_pipe->stop();

    // reset internal state
    {
        std::scoped_lock lock(m_mutex);

        m_channels.clear();
        m_clients.clear();
    }

    return APP_EXITCODE_OK;
}


void svc_worker::process_received_data()
{
    std::scoped_lock lock(m_mutex);

    std::set<pipe_token_t> channels_to_erase;
    bytes_t packet;

    ResetEvent(m_recv_event);

    packet.reserve(cix::win_namedpipe_server::io_buffer_default_size);

    for (auto& [ pipe_token, channel ] : m_channels)
    {
        if (channel->data_recv && !channel->input_buffer.empty())
        {
            bool must_erase = false;

            while (this->process_channel_received_data(
                channel, packet, &must_erase) && !must_erase)
            { ; }

            if (must_erase)
                channels_to_erase.insert(pipe_token);
        }
    }

    for (const auto& pipe_token : channels_to_erase)
        this->erase_channel_and_client(pipe_token, true);
}


bool svc_worker::process_channel_received_data(
    std::shared_ptr<channel_t> channel, bytes_t& packet, bool* out_must_erase)
{
    packet.clear();
    *out_must_erase = false;

    const auto proto_error = proto::extract_next_packet(
        channel->input_buffer, packet);

    switch (proto_error)
    {
        case proto::ok:
            break;

        case proto::error_incomplete:
            return false;  // stop processing

        default:
            assert(0);
        case proto::error_malformed:
        case proto::error_garbage:
        case proto::error_toobig:
        case proto::error_crc:
            // disconnect as a reply to malformed packets
            *out_must_erase = true;
            return false;  // stop processing
    }

    const auto& header = *reinterpret_cast<proto::header_t*>(packet.data());

    this->process_channel_received_packet(
        channel, packet, header, out_must_erase);

    // continue processing only if *must_erase* flag not raised
    return !*out_must_erase;
}


void svc_worker::process_channel_received_packet(
    std::shared_ptr<channel_t> channel,
    const bytes_t& packet,
    const proto::header_t& header,
    bool* out_must_erase)
{
    // first packet is expected to be *op_channel_setup*, and *op_channel_setup*
    // must be sent only once during connection lifetime
    if (channel->is_just_connected())
    {
        if (header.opcode != proto::op_channel_setup)
        {
            *out_must_erase = true;
        }
        else
        {
            this->process_channel_setup(
                channel, packet, header, out_must_erase);
        }

        return;
    }
    else if (header.opcode == proto::op_channel_setup)
    {
        // client should not send this twice
        *out_must_erase = true;
        return;
    }

    switch (header.opcode)
    {
        case proto::op_channel_setup:
            // case already handled above
            assert(0);
            return;

        case proto::op_channel_setup_ack:
            // client should not send this
            *out_must_erase = true;
            return;

        case proto::op_status:
            assert(0);  // server-side does not need to handle this
            return;

        case proto::op_ping:
            this->process_channel_received_ping_packet(
                channel, header, out_must_erase);
            return;

        case proto::op_socks:
            this->process_channel_received_socks_packet(
                channel, packet, header, out_must_erase);
            return;

        case proto::op_socks_close:
        case proto::op_socks_disconnected:
            this->process_channel_received_socks_close_packet(
                channel, packet, header, out_must_erase);
            return;

        case proto::op_uninstall_self:
            this->process_channel_received_uninstall_self_packet();
            return;

        default:
        {
            assert(0);
            auto write_channel = this->find_write_channel(channel);
            if (write_channel)
            {
                write_channel->send(
                    m_pipe,
                    proto::make_status(header.uid, proto::status_unsupported));
            }
            else
            {
                *out_must_erase = true;
            }
            return;
        }
    }
}


void svc_worker::process_channel_setup(
    std::shared_ptr<channel_t> channel,
    const bytes_t& packet,
    const proto::header_t& header,
    bool* out_must_erase)
{
    assert(channel->is_just_connected());

    auto _configure_channel =
        [](std::shared_ptr<channel_t> channel,
            clientid_t client_id,
            proto::channel_setup_flags_t flags)
        {
            assert(channel->client_id == proto::invalid_client_id);
            assert(channel->config_flags == chanconfig_none);

            channel->client_id = client_id;
            channel->config_flags = chanconfig_none;

            if (flags & proto::chansetup_read)  // client-side
                channel->config_flags |= chanconfig_write;  // server-side

            if (flags & proto::chansetup_write)
                channel->config_flags |= chanconfig_read;
        };

    // note: proto::payload_channel_setup_t values already net2host()'ed by
    // proto::extract_next_packet()
    const auto& payload = *reinterpret_cast<
        const proto::payload_channel_setup_t*>(
            packet.data() + sizeof(proto::header_t));

    clientid_t client_id = proto::invalid_client_id;

    if (payload.client_id == 0)  // new client
    {
        do { client_id = proto::generate_client_id(); }
        while (m_clients.find(client_id) != m_clients.end());

        _configure_channel(channel, client_id, payload.flags);

        // create client object (client id was null)
        auto chan_read =
            (channel->config_flags & chanconfig_read) ? channel : nullptr;
        auto chan_write =
            (channel->config_flags & chanconfig_write) ? channel : nullptr;
        m_clients.emplace(
            client_id,
            std::make_shared<client_t>(client_id, chan_read, chan_write));
    }
    else
    {
        auto client_it = m_clients.find(payload.client_id);

        if (client_it == m_clients.end())
        {
            *out_must_erase = true;
            return;
        }
        else
        {
            auto client = client_it->second;

            assert(client->id == client_it->first);
            client_id = payload.client_id;

            // ensure client is not already setup, or that there is no setup
            // collision
            if ((payload.flags & proto::chansetup_read && client->chan_write) ||
                (payload.flags & proto::chansetup_write && client->chan_read))
            {
                *out_must_erase = true;
                return;
            }

            _configure_channel(channel, payload.client_id, payload.flags);

            if (channel->config_flags & chanconfig_read)
            {
                assert(!client->chan_read);
                client->chan_read = channel;
            }

            if (channel->config_flags & chanconfig_write)
            {
                assert(!client->chan_write);
                client->chan_write = channel;
            }
        }
    }

    assert(client_id != proto::invalid_client_id);
    if (client_id != proto::invalid_client_id)
    {
        // bypass config flags validation for this one time because the client
        // expects an ack from us
        channel->send(
            m_pipe,
            proto::make_channel_setup_ack(header.uid, client_id),
            false);  // bypass config flags
    }
}


void svc_worker::process_channel_received_ping_packet(
    std::shared_ptr<channel_t> channel,
    const proto::header_t& header,
    bool* out_must_erase)
{
    auto write_channel = this->find_write_channel(channel);

    if (write_channel)
    {
        write_channel->send(
            m_pipe,
            proto::make_status(header.uid, proto::status_ok));
    }
    else
    {
        *out_must_erase = true;
    }
}


void svc_worker::process_channel_received_socks_packet(
    std::shared_ptr<channel_t> channel,
    const bytes_t& packet,
    const proto::header_t& header,
    bool* out_must_erase)
{
    CIX_UNVAR(header);

    // note: proto::payload_socks_header_t values already net2host()'ed by
    // proto::extract_next_packet()
    const auto socks_id =
        reinterpret_cast<const proto::payload_socks_header_t*>(
            packet.data() + sizeof(proto::header_t))->socks_id;

    if (socks_id == proto::invalid_socks_id)
        return;  // noop

    const auto socks_payload_size =
        packet.size() -
        sizeof(proto::header_t) -
        sizeof(proto::payload_socks_header_t);

    // paranoid check
    if (socks_payload_size == 0 || socks_payload_size >= packet.size())
    {
        // this is a *proto*-related error so disconnect client
        *out_must_erase = true;
        return;
    }

    const auto* socks_payload =
        packet.data() +
        sizeof(proto::header_t) +
        sizeof(proto::payload_socks_header_t);

    cix::lock_guard lock(m_mutex);

    auto client = this->find_client_by_channel(channel);
    if (!client)
    {
        // SOCKS request received but client_t is not configured yet,
        // or has been disconnected
        *out_must_erase = true;
        return;
    }

    // CAUTION:
    // * *socks_id* and *socks_token* are different values with different
    //   purpose
    // * *socks_id* is the ID provided by the remote client-side to identify a
    //   given SOCKS connection on its side
    // * *socks_token* is the ID of this same SOCKS connection, as randomly
    //   generated and managed by the socks_proxy object
    // * in other words, *socks_id* identifies a SOCKS connection on the
    //   client-side while *socks_token* identifies this same SOCKS connection
    //   on the server-side, so that there is always one *socks_id* for one
    //   *socks_token*
    // * the reason behind this distinction is because multiple remote clients
    //   can be connected to this svc_worker and we must to ensure a same ID of
    //   SOCKS connection IS NOT shared among multiple clients
    socks_proxy::token_t socks_token;

    auto socks_token_it = client->socks_id_to_token.find(socks_id);
    if (socks_token_it == client->socks_id_to_token.end())
    {
        // here, this is a new SOCKS ID so a new connection must be opened
        socks_token = m_socks_proxy->create_client();
        if (socks_token == socks_proxy::invalid_token)
        {
            // socks_proxy failed to create a new connection
            // close the client-side connection because this situation can only
            // happen under abnormal circumstances
            *out_must_erase = true;
            return;
        }

        assert(
            m_socks_token_to_client.find(socks_token) ==
            m_socks_token_to_client.end());

        // map socks_id to its socks_token counterpart
        client->socks_id_to_token[socks_id] = socks_token;
        m_socks_token_to_client[socks_token] = client;
    }
    else
    {
        socks_token = socks_token_it->second;
    }

    bytes_t socks_packet;

    socks_packet.resize(socks_payload_size);
    std::memcpy(socks_packet.data(), socks_payload, socks_payload_size);

    lock.unlock();

    m_socks_proxy->push_request(socks_token, std::move(socks_packet));
}


void svc_worker::process_channel_received_socks_close_packet(
    std::shared_ptr<channel_t> channel,
    const bytes_t& packet,
    const proto::header_t& header,
    bool* out_must_erase)
{
    // note: proto::payload_socks_header_t values already net2host()'ed by
    // proto::extract_next_packet()
    const auto socks_id =
        reinterpret_cast<const proto::payload_socks_header_t*>(
            packet.data() + sizeof(proto::header_t))->socks_id;

    cix::lock_guard lock(m_mutex);

    auto client = this->find_client_by_channel(channel);
    if (!client)
    {
        *out_must_erase = true;
        return;
    }

    const auto socks_token = client->find_socks_token_by_id(socks_id);

    if (client->chan_write)
    {
        client->chan_write->send(
            m_pipe,
            proto::make_status(header.uid, proto::status_ok));
    }

    lock.unlock();

    if (socks_token != socks_proxy::invalid_token)
        m_socks_proxy->disconnect_client(socks_token);
}


void svc_worker::process_channel_received_uninstall_self_packet()
{
#ifdef APP_ENABLE_SERVICE
    auto owner = svc::instance();
    const auto uninstall_svc = !owner || owner->running_as_service();
    owner.reset();

    if (uninstall_svc)
        svc::uninstall({}, false);
#endif

    // TODO: enable a self-erase flag to be read by wmain()???

    SetEvent(m_stop_event);
}


#if 0
std::shared_ptr<svc_worker::client_t>
svc_worker::find_client_by_pipe_token(pipe_token_t pipe_token) const
{
    std::scoped_lock lock(m_mutex);

    auto chan_it = m_channels.find(pipe_instance_token);
    if (chan_it != m_channels.end())
    {
        auto& channel = chan_it->second;
        return this->find_client_by_channel(channel);
    }

    return nullptr;
}
#endif


std::shared_ptr<svc_worker::client_t>
svc_worker::find_client_by_channel(std::shared_ptr<channel_t> channel) const
{
    if (!channel)
    {
        assert(0);
        return nullptr;
    }

    // channel+client not configured yet?
    if (channel->client_id == proto::invalid_client_id)
        return nullptr;

    std::scoped_lock lock(m_mutex);

    auto client_it = m_clients.find(channel->client_id);
    if (client_it == m_clients.end())
        return nullptr;

    return client_it->second;
}


std::shared_ptr<svc_worker::client_t>
svc_worker::find_client_by_socks_token(socks_proxy::token_t socks_token)
{
    std::scoped_lock lock(m_mutex);

    auto it = m_socks_token_to_client.find(socks_token);
    if (it == m_socks_token_to_client.end())
        return nullptr;

    auto client = it->second.lock();
    if (!client)
    {
        m_socks_token_to_client.erase(it);
        return nullptr;
    }

    return client;
}


std::shared_ptr<svc_worker::channel_t>
svc_worker::find_write_channel(std::shared_ptr<channel_t> channel) const
{
    if (!channel)
    {
        assert(0);
        return nullptr;
    }

    std::scoped_lock lock(m_mutex);

    if (channel->client_id == proto::invalid_client_id)
        return channel;  // if channel is not setup yet

    const auto client_it = m_clients.find(channel->client_id);
    if (client_it == m_clients.end())
    {
        assert(0);
        // if (channel->config_flags & chanconfig_write)
        //     return channel;
        return nullptr;
    }

    const auto client = client_it->second;

    assert(channel == client->chan_write || channel == client->chan_read);

    if (client->chan_write)
        return client->chan_write;

    assert(0);
    // return channel;
    return nullptr;
}


void svc_worker::erase_channel_and_client(
    pipe_token_t pipe_token, bool disconnect)
{
    // erase a channel as well as its parent client_t object and related channel
    // if any

    cix::lock_guard lock(m_mutex);

    auto chan_it = m_channels.find(pipe_token);
    if (chan_it != m_channels.end())
    {
        const auto client_id = chan_it->second->client_id;

        if (client_id == proto::invalid_client_id)
        {
            // here, channel was still pending to be setup so it is not attached
            // yet to a client_t object

            if (disconnect && m_pipe)
                chan_it->second->disconnect(m_pipe);

            m_channels.erase(chan_it);
            chan_it = m_channels.end();

            lock.unlock();  // symmetry with block below
        }
        else
        {
            // m_channels is modified by erase_client() so release refs to it
            chan_it = m_channels.end();

            lock.unlock();

            this->erase_client(
                client_id,
                disconnect,
                disconnect ? 0 : pipe_token);
        }
    }
}


void svc_worker::erase_client(
    clientid_t client_id,
    bool disconnect,
    pipe_token_t disconnect_except_pipe_token)
{
    // erase a client and its channel(s) from local structures

    if (client_id == proto::invalid_client_id)
        return;

    cix::lock_guard lock(m_mutex);

    std::set<socks_proxy::token_t> socks_tokens_to_disconnect;

    auto client_it = m_clients.find(client_id);
    if (client_it != m_clients.end())
    {
        auto client = client_it->second;
        pipe_token_t read_token = 0;
        pipe_token_t write_token = 0;

        if (!client->socks_id_to_token.empty())
        {
            // hold the list of related SOCKS IDs locally so we can
            // m_socks_proxy->disconnect_client() separately later on,
            // so that we can unlock our mutex to avoid any stall due to the
            // on_socks_disconnected() callback being called during a
            // disconnect_client() call

            assert(socks_tokens_to_disconnect.empty());
            for (auto it : client->socks_id_to_token)
            {
                const auto socks_token = it.second;

                socks_tokens_to_disconnect.insert(socks_token);
                m_socks_token_to_client.erase(socks_token);
            }

            client->socks_id_to_token.clear();
        }

        if (client->chan_read)
            read_token = client->chan_read->pipe_token;

        if (client->chan_write)
            write_token = client->chan_write->pipe_token;

        if (disconnect && m_pipe)
            client->disconnect(m_pipe, disconnect_except_pipe_token);

        if (read_token != 0)
            m_channels.erase(read_token);

        if (write_token != 0 && write_token != read_token)
            m_channels.erase(write_token);

        client.reset();
        m_clients.erase(client_it);
        client_it = m_clients.end();
    }

    if (!socks_tokens_to_disconnect.empty())
    {
        auto socks_proxy = m_socks_proxy;

        lock.unlock();  // IMPORTANT, see explanation above

        for (const auto socks_id : socks_tokens_to_disconnect)
            socks_proxy->disconnect_client(socks_id);
    }
    else
    {
        lock.unlock();  // just for symmetry with the if() block above
    }
}


void svc_worker::disconnect_all()
{
    std::scoped_lock lock(m_mutex);

    if (m_pipe)
    {
        for (auto& chan_it : m_channels)
            chan_it.second->disconnect(m_pipe);
    }
}


void svc_worker::on_namedpipe_connected(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    cix::win_namedpipe_server::instance_token_t pipe_instance_token)
{
    assert(pipe);
    assert(pipe_instance_token);
    assert(pipe == m_pipe || !m_pipe);

    CIX_UNVAR(pipe);

    LOGTRACE("PIPE INSTANCE CONNECTED");

    // force-cleanup any existing channel with a same token, as well as any
    // client object that depends on it
    this->erase_channel_and_client(pipe_instance_token, false);
}


void svc_worker::on_namedpipe_recv(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    cix::win_namedpipe_server::instance_token_t pipe_instance_token,
    cix::win_namedpipe_server::bytes_t&& packet)
{
    assert(pipe);
    assert(!packet.empty());
    assert(pipe == m_pipe || !m_pipe);

    LOGTRACE("PIPE INSTANCE RECV {} bytes", packet.size());

    // this is a callback method, it must be executed as fast as possible so
    // just (create and) feed the channel_t object here but leave the parsing
    // and other actions to the maintenance thread

    if (!packet.empty())
    {
        std::scoped_lock lock(m_mutex);

        auto chan_it = m_channels.find(pipe_instance_token);
        if (chan_it != m_channels.end())
        {
            auto& channel = chan_it->second;
            channel->feed(std::move(packet));
        }
        else
        {
            auto channel = std::make_shared<channel_t>(
                pipe_instance_token, std::move(packet));

            m_channels.insert(std::make_pair(pipe_instance_token, channel));
        }

        SetEvent(m_recv_event);
    }
}


void svc_worker::on_namedpipe_sent(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    cix::win_namedpipe_server::instance_token_t pipe_instance_token,
    cix::win_namedpipe_server::bytes_t&& packet,
    std::size_t output_queue_size)
{
    CIX_UNVAR(pipe);
    CIX_UNVAR(pipe_instance_token);
    CIX_UNVAR(packet);
    CIX_UNVAR(output_queue_size);

    assert(pipe);
    assert(!packet.empty());
    assert(pipe == m_pipe || !m_pipe);

    LOGTRACE("PIPE INSTANCE WROTE {} bytes", packet.size());
}


void svc_worker::on_namedpipe_closed(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    cix::win_namedpipe_server::instance_token_t pipe_instance_token)
{
    CIX_UNVAR(pipe);

    assert(pipe);
    assert(pipe_instance_token);
    assert(pipe == m_pipe || !m_pipe);

    LOGTRACE("PIPE INSTANCE DISCONNECTED");

    this->erase_channel_and_client(pipe_instance_token, true);
}


void svc_worker::on_socks_response(
    std::shared_ptr<socks_proxy> socks_proxy,
    std::shared_ptr<socks_proxy::socks_packet_t> response)
{
    CIX_UNVAR(socks_proxy);

    const auto socks_token = response->client_token;

    cix::lock_guard lock(m_mutex);

    auto client = this->find_client_by_socks_token(socks_token);
    if (!client)
    {
        // no client found, disconnect from SOCKS target
        lock.unlock();
        m_socks_proxy->disconnect_client(socks_token);
        return;
    }

    const auto socks_id = client->find_socks_id_by_token(socks_token);
    if (socks_id == proto::invalid_socks_id)
    {
        assert(0);
        lock.unlock();
        m_socks_proxy->disconnect_client(socks_token);
        return;
    }

    if (!response->packet.empty() && client->chan_write)
    {
        // forward packet to the client side
        client->chan_write->send(
            m_pipe, proto::make_socks(socks_id, response->packet));
    }
}


void svc_worker::on_socks_close_client(
    std::shared_ptr<socks_proxy> socks_proxy,
    socks_proxy::token_t socks_token)
{
    std::scoped_lock lock(m_mutex);

    CIX_UNVAR(socks_proxy);

    auto it = m_socks_token_to_client.find(socks_token);
    if (it == m_socks_token_to_client.end())
        return;

    auto client = it->second.lock();
    if (!client)
    {
        m_socks_token_to_client.erase(it);
        return;
    }

    const auto socks_id = client->find_socks_id_by_token(socks_token);
    if (socks_id == proto::invalid_socks_id)
    {
        assert(0);
        return;
    }

    if (client->chan_write)
        client->chan_write->send(m_pipe, proto::make_socks_close(socks_id));
}


void svc_worker::on_socks_disconnected(
    std::shared_ptr<socks_proxy> socks_proxy,
    socks_proxy::token_t socks_token)
{
    std::scoped_lock lock(m_mutex);

    CIX_UNVAR(socks_proxy);

    auto it = m_socks_token_to_client.find(socks_token);
    if (it == m_socks_token_to_client.end())
        return;

    auto client = it->second.lock();
    if (!client)
    {
        m_socks_token_to_client.erase(it);
        return;
    }

    const auto socks_id = client->find_socks_id_by_token(socks_token);
    if (socks_id == proto::invalid_socks_id)
    {
        assert(0);
        return;
    }

    if (client->chan_write)
    {
        client->chan_write->send(
            m_pipe, proto::make_socks_disconnected(socks_id));
    }
}



//******************************************************************************



svc_worker::channel_t::channel_t(pipe_token_t pipe_token_, bytes_t&& packet)
    : client_id{proto::invalid_client_id}
    , config_flags{chanconfig_none}
    , pipe_token{pipe_token_}
    , input_buffer(std::move(packet))
    , last_recv{0}
    , data_recv{false}
{
    assert(pipe_token_ != 0);

    if (!input_buffer.empty())
    {
        last_recv = cix::ticks_now();
        data_recv = true;
    }
}


bool svc_worker::channel_t::is_just_connected() const
{
    return
        client_id == proto::invalid_client_id ||
        config_flags == chanconfig_none;
}


void svc_worker::channel_t::feed(bytes_t&& packet)
{
    // CAUTION: favor copy() here, instead of any seemingly more optimized way,
    // like swap for instance, so that we keep the benefit of any potential
    // input_buffer alloc made by previous calls

    if (!packet.empty())
    {
        // if (input_buffer.empty())
        // {
        //     input_buffer.swap(packet);
        // }
        // else
        // {
            input_buffer.reserve(input_buffer.size() + packet.size());
            std::copy(
                packet.begin(), packet.end(),
                std::back_inserter(input_buffer));
        // }

        last_recv = cix::ticks_now();
        data_recv = true;
    }
}


bool svc_worker::channel_t::send(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    bytes_t&& packet,
    bool validate_config_first)
{
    if (pipe_token == 0 || !pipe)
        return false;

    if (packet.empty())
        return true;  // not an error per se

    if (validate_config_first &&
        config_flags != chanconfig_none &&
        !(config_flags & chanconfig_write))
    {
        assert(0);  // not supposed to send anything through this channel
        return false;
    }

    return pipe->send(pipe_token, std::move(packet));
}


void svc_worker::channel_t::disconnect(
    std::shared_ptr<cix::win_namedpipe_server> pipe)
{
    if (pipe && pipe_token != 0)
        pipe->disconnect_instance(pipe_token);

    pipe_token = 0;
    input_buffer.clear();
    data_recv = false;
}



//******************************************************************************



svc_worker::client_t::client_t(
    clientid_t id_,
    std::shared_ptr<channel_t> chan_read_,
    std::shared_ptr<channel_t> chan_write_)
: id{id_}
, chan_read{chan_read_}
, chan_write{chan_write_}
{
    assert(id_ != proto::invalid_client_id);
    assert(chan_read_ || chan_write_);
}


void svc_worker::client_t::disconnect(
    std::shared_ptr<cix::win_namedpipe_server> pipe,
    pipe_token_t except_pipe_token)
{
    if (pipe)
    {
        pipe_token_t read_token = 0;

        if (chan_read &&
            (except_pipe_token == 0 ||
                except_pipe_token != chan_read->pipe_token))
        {
            read_token = chan_read->pipe_token;
            chan_read->disconnect(pipe);
        }

        if (chan_write &&
            chan_write->pipe_token != read_token &&
            (except_pipe_token == 0 ||
                except_pipe_token != chan_write->pipe_token))
        {
            chan_write->disconnect(pipe);
        }
    }

    chan_read.reset();
    chan_write.reset();
}


socks_proxy::token_t
svc_worker::client_t::find_socks_token_by_id(proto::socksid_t socks_id) const
{
    auto it = socks_id_to_token.find(socks_id);

    return
        it == socks_id_to_token.end() ?
        socks_proxy::invalid_token :
        it->second;
}


proto::socksid_t
svc_worker::client_t::find_socks_id_by_token(
    socks_proxy::token_t socks_token) const
{
    // for (const auto& pair : socks_id_to_token)
    // {
    //     if (pair.second == socks_token)
    //         return pair.first;
    // }
    // return proto::invalid_socks_id;

    auto it = std::find_if(
        socks_id_to_token.begin(), socks_id_to_token.end(),
        [&socks_token](const auto& pair) { return pair.second == socks_token; });

    return
        it == socks_id_to_token.end() ?
        proto::invalid_socks_id :
        it->first;
}
