// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


// A SOCKS server that takes its input data from memory via a method call.
//
// Properties:
// * SOCKS5 only
// * TCP only
// * Only CONNECT command supported
// * IPv4, IPv6 and domain name addressing supported
//
// TODO / FIXME: push_request() assumes the *packet* arg contains exactly one
// SOCKS request. That is, the content of *packet* is assumed not to be
// truncated.
//
class socks_proxy :
    public std::enable_shared_from_this<socks_proxy>,
    public socketio::listener_t
{
public:
    typedef proto::socksid_t token_t;
    typedef std::vector<std::uint8_t> bytes_t;

    enum : token_t { invalid_token = 0 };

    enum socks_auth_t : bytes_t::value_type
    {
        socks_noauth = 0,
        // socks_auth_gssapi = 1,
        socks_auth_userpass = 2,
        socks_auth_no_supported_method = 0xff,
    };

    enum socks_cmd_t : bytes_t::value_type
    {
        socks_cmd_connect = 1,
        // socks_cmd_bind = 2,
        // socks_cmd_udp_associate = 3,
    };

    enum socks_addr_t : bytes_t::value_type
    {
        socks_addr_ipv4 = 1,
        socks_addr_name = 3,
        socks_addr_ipv6 = 4,
    };

    enum socks_reply_code_t : bytes_t::value_type
    {
        socks_reply_success = 0,
        socks_reply_general_failure = 1,         // general SOCKS server failure
        socks_reply_conn_not_allowed = 2,        // connection not allowed by ruleset
        socks_reply_net_unreachable = 3,         // network unreachable
        socks_reply_host_unreachable = 4,        // host unreachable
        socks_reply_conn_refused = 5,            // connection refused
        socks_reply_ttl_expired = 6,             // TTL expired
        socks_reply_command_not_supported = 7,   // command not supported
        socks_reply_addr_type_not_supported = 8, // address type not supported
    };

    struct socks_packet_t
    {
        socks_packet_t() = delete;
        socks_packet_t(token_t token, bytes_t&& packet_)
            : client_token{token}
            , packet(std::move(packet_))
            , when{cix::ticks_now()}
            { }

        token_t client_token;
        bytes_t packet;
        cix::ticks_t when;
    };

    struct listener_t
    {
        // got a SOCKS response from either socks_proxy itself or remote SOCKS
        // target
        virtual void on_socks_response(
            std::shared_ptr<socks_proxy> socks_proxy,
            std::shared_ptr<socks_proxy::socks_packet_t> response) = 0;

        // socks_proxy requires the client side to be disconnected
        // typically due to invalid / malformed SOCKS packet
        virtual void on_socks_close_client(
            std::shared_ptr<socks_proxy> socks_proxy,
            socks_proxy::token_t socks_token) = 0;

        // connection with SOCKS target closed
        virtual void on_socks_disconnected(
            std::shared_ptr<socks_proxy> socks_proxy,
            socks_proxy::token_t socks_token) = 0;
    };

private:
    enum : DWORD
    {
        socket_connect_timeout = 6000,
        socket_io_timeout = 4000,
    };

    enum socks_state_t
    {
        socks_state_newclient,
        socks_state_needauth,
        socks_state_needcmd,    // (no)auth'ed, now waiting for CONNECT command
        socks_state_connected,  // passed CONNECT command handling
    };

    struct client_t
    {
        token_t token;
        socks_state_t socks_state;
        SOCKET conn;  // client connection with SOCKS target
        std::string remote_label;
        cix::ticks_t last_activity;
    };

public:
    socks_proxy();
    ~socks_proxy();

    void set_listener(std::shared_ptr<listener_t> listener);

    void launch();

    token_t create_client();
    void push_request(token_t client_token, bytes_t&& packet);
    void disconnect_client(token_t client_token);

    void stop();

public:
    void maintenance_thread();
    void handle_requests();
    void handle_socks_request(
        std::shared_ptr<client_t> client,
        socks_packet_t& request);
    bool handle_socks_request__newclient(
        client_t& client,
        const socks_packet_t& request);
    bool handle_socks_request__needauth(
        client_t& client,
        const socks_packet_t& request);
    bool handle_socks_request__needcmd(
        client_t& client,
        const socks_packet_t& request);
    bool handle_socks_request__connected(
        const client_t& client,
        socks_packet_t& request);

    void send_to_client(client_t& client, bytes_t&& packet);
    void send_reply_to_client(
        client_t& client, socks_reply_code_t code, socks_addr_t addr_type);
    bool send_to_target(const client_t& client, bytes_t&& packet);
    std::shared_ptr<client_t> find_client(SOCKET socket) const;
    void erase_client(token_t client_token);
    void disconnect_socket(SOCKET socket);
    void notify_response(std::shared_ptr<socks_packet_t> response);
    void request_close(token_t client_token);
    void notify_disconnected(token_t client_token);

    // socketio::listener_t
    void on_socketio_recv(SOCKET socket, bytes_t&& packet);
    void on_socketio_disconnected(SOCKET socket);

    static socks_reply_code_t wsaerror_to_socks_reply(int error);

    static int resolve(
        const char* host,
        unsigned short port,
        struct addrinfo** out_addr,
        int ai_family=AF_UNSPEC,
        int ai_flags=0);  // AI_PASSIVE

    static socks_reply_code_t connect_socket(
        client_t& client, struct addrinfo* remote_addr);

public:
    mutable std::recursive_mutex m_mutex;
    cix::random::fast m_rand;
    std::unique_ptr<std::thread> m_thread;
    HANDLE m_stop_event;
    HANDLE m_request_event;

    std::shared_ptr<socketio> m_socketio;
    std::weak_ptr<listener_t> m_listener;

    std::list<std::shared_ptr<socks_packet_t>> m_pending_requests;

    std::map<token_t, std::shared_ptr<client_t>> m_clients;
};
