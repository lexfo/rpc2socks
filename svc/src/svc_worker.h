// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


class svc_worker :
    public cix::win_namedpipe_server::listener_t,
    public socks_proxy::listener_t,
    public std::enable_shared_from_this<svc_worker>
{
public:
    enum channel_config_t : std::uint32_t
    {
        chanconfig_none   = 0,
        chanconfig_read   = 0x01,  // server's end
        chanconfig_write  = 0x02,  // server's end
        chanconfig_duplex = chanconfig_read | chanconfig_write,
    };

private:
    typedef proto::clientid_t clientid_t;
    typedef proto::bytes_t bytes_t;
    typedef cix::win_namedpipe_server::instance_token_t pipe_token_t;

    struct channel_t
    {
        channel_t() = delete;
        channel_t(pipe_token_t pipe_token_, bytes_t&& packet);
        ~channel_t() = default;

        bool is_just_connected() const;
        void feed(bytes_t&& packet);
        bool send(
            std::shared_ptr<cix::win_namedpipe_server> pipe,
            bytes_t&& packet,
            bool validate_config_first=true);
        void disconnect(std::shared_ptr<cix::win_namedpipe_server> pipe);

        clientid_t client_id;
        channel_config_t config_flags;
        pipe_token_t pipe_token;
        bytes_t input_buffer;
        cix::ticks_t last_recv;
        bool data_recv;
    };

    struct client_t
    {
        client_t() = delete;
        client_t(
            clientid_t id_,
            std::shared_ptr<channel_t> chan_read_,
            std::shared_ptr<channel_t> chan_write_);
        ~client_t() = default;

        void disconnect(
            std::shared_ptr<cix::win_namedpipe_server> pipe,
            pipe_token_t except_pipe_token=0);

        socks_proxy::token_t find_socks_token_by_id(
            proto::socksid_t socks_id) const;

        proto::socksid_t find_socks_id_by_token(
            socks_proxy::token_t socks_token) const;

        clientid_t id;
        std::shared_ptr<channel_t> chan_read;
        std::shared_ptr<channel_t> chan_write;
        std::map<proto::socksid_t, socks_proxy::token_t> socks_id_to_token;
    };

public:
    svc_worker();
    ~svc_worker();

    exit_t init(HANDLE stop_event, const std::wstring& pipe_base_name);
    exit_t main_loop();

private:
    // main loop subs
    void process_received_data();
    bool process_channel_received_data(
        std::shared_ptr<channel_t> channel,
        bytes_t& packet,
        bool* out_must_erase);
    void process_channel_received_packet(
        std::shared_ptr<channel_t> channel,
        const bytes_t& packet,
        const proto::header_t& header,
        bool* out_must_erase);
    void process_channel_setup(
        std::shared_ptr<channel_t> channel,
        const bytes_t& packet,
        const proto::header_t& header,
        bool* out_must_erase);
    void process_channel_received_ping_packet(
        std::shared_ptr<channel_t> channel,
        const proto::header_t& header,
        bool* out_must_erase);
    void process_channel_received_socks_packet(
        std::shared_ptr<channel_t> channel,
        const bytes_t& packet,
        const proto::header_t& header,
        bool* out_must_erase);
    void process_channel_received_socks_close_packet(
        std::shared_ptr<channel_t> channel,
        const bytes_t& packet,
        const proto::header_t& header,
        bool* out_must_erase);
    void process_channel_received_uninstall_self_packet();

    // utils
    // std::shared_ptr<client_t> find_client_by_pipe_token(
    //     pipe_token_t pipe_token) const;
    std::shared_ptr<client_t> find_client_by_channel(
        std::shared_ptr<channel_t> channel) const;
    std::shared_ptr<client_t> find_client_by_socks_token(
        socks_proxy::token_t socks_token);
    std::shared_ptr<channel_t> find_write_channel(
        std::shared_ptr<channel_t> channel) const;
    void erase_channel_and_client(pipe_token_t pipe_token, bool disconnect);
    void erase_client(
        clientid_t client_id,
        bool disconnect,
        pipe_token_t disconnect_except_pipe_token=0);
    void disconnect_all();

    // cix::win_namedpipe_server::listener_t
    void on_namedpipe_connected(
        std::shared_ptr<cix::win_namedpipe_server> pipe,
        cix::win_namedpipe_server::instance_token_t pipe_instance_token);
    void on_namedpipe_recv(
        std::shared_ptr<cix::win_namedpipe_server> pipe,
        cix::win_namedpipe_server::instance_token_t pipe_instance_token,
        cix::win_namedpipe_server::bytes_t&& packet);
    void on_namedpipe_sent(
        std::shared_ptr<cix::win_namedpipe_server> pipe,
        cix::win_namedpipe_server::instance_token_t pipe_instance_token,
        cix::win_namedpipe_server::bytes_t&& packet,
        std::size_t output_queue_size);
    void on_namedpipe_closed(
        std::shared_ptr<cix::win_namedpipe_server> pipe,
        cix::win_namedpipe_server::instance_token_t pipe_instance_token);

    // socks_proxy::listener_t
    void on_socks_response(
        std::shared_ptr<socks_proxy> socks_proxy,
        std::shared_ptr<socks_proxy::socks_packet_t> response);
    void on_socks_close_client(
        std::shared_ptr<socks_proxy> socks_proxy,
        socks_proxy::token_t socks_token);
    void on_socks_disconnected(
        std::shared_ptr<socks_proxy> socks_proxy,
        socks_proxy::token_t socks_token);

private:
    mutable std::recursive_mutex m_mutex;

    HANDLE m_stop_event;
    HANDLE m_recv_event;
    std::wstring m_pipe_path;
    std::shared_ptr<cix::win_namedpipe_server> m_pipe;
    std::shared_ptr<socks_proxy> m_socks_proxy;

    std::map<pipe_token_t, std::shared_ptr<channel_t>> m_channels;
    std::map<clientid_t, std::shared_ptr<client_t>> m_clients;
    std::map<socks_proxy::token_t, std::weak_ptr<client_t>> m_socks_token_to_client;
};

CIX_IMPLEMENT_ENUM_BITOPS(svc_worker::channel_config_t)
