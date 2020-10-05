// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


// R/IPC protocol
//
// * little-endian
// * not designed for embedded devices
//
// Note on *op_channel_setup* opcode:
//
// Clients are REQUIRED to send an *op_channel_setup* packet before any further
// attempt to use an established connection.
//
// Typical client with support of asynchronous operations uses both
// *chansetup_read* and *chansetup_write* flags, which are NOT mutually
// exclusive.
//
// This opcode mainly exists so that clients with limited support of
// asynchronous I/O operations can still use this service in a synchronous
// fashion and without significant loss of speed performance by allowing to open
// two communication channels instead of one. Being respectively opened for
// read-only and write-only operations - although the underlying transport layer
// still allows duplex communication technically-wise, in the case of this
// particular service (rpc2socks).
//
// By sending an *op_channel_setup* packet, the client-side can force a
// particular instance of a pipe to act like if it were in read-only or
// write-only mode, so that a client with limited asynchronous I/O support can
// connect twice to a pipe and have two threads running, respectively for
// read-only and write-only operations.
namespace proto {

typedef std::uint8_t byte_t;
typedef std::vector<byte_t> bytes_t;

// proto client identifier
typedef std::uint64_t clientid_t;
static constexpr clientid_t invalid_client_id = 0;

// SOCKS connection identifier
typedef std::uint64_t socksid_t;
static constexpr socksid_t invalid_socks_id = 0;

enum error_t : unsigned
{
    ok = 0,
    error_garbage = 1,     // no packet found in buffer
    error_incomplete = 2,  // packet incomplete
    error_malformed = 3,   // unexpected packet content and/or size
    error_toobig = 4,      // header.len too big
    error_crc = 5,         // header crc32 mismatch
};

enum opcode_t : std::uint8_t
{
    op_channel_setup = 1,
    op_channel_setup_ack = 2,
    op_status = 5,
    op_ping = 10,
    op_socks = 150,               // sent by client or server side
    op_socks_close = 151,         // sent by client or server side
    op_socks_disconnected = 152,  // sent by client or server side
    op_uninstall_self = 240,
};

enum status_t : std::uint8_t
{
    status_ok = 0,
    status_unsupported = 1,  // e.g. unsupported opcode
};

enum channel_setup_flags_t : std::uint32_t
{
    chansetup_read   = 0x01,  // client uses this channel to read data
    chansetup_write  = 0x02,  // client uses this channel to write data
    chansetup_duplex = chansetup_read | chansetup_write,
};
CIX_IMPLEMENT_ENUM_BITOPS(channel_setup_flags_t)


// xkcd221 section
static constexpr std::array<byte_t, 4> magic = { 0xe4, 0x85, 0xb4, 0xb2 };
static constexpr std::size_t max_packet_size = 16 * 1024 * 1024;  // see also max_payload_size defined below


#pragma pack(push, 1)
struct header_t
{
    byte_t magic[proto::magic.size()];  // proto::magic
    std::uint32_t len;    // total packet length (bytes), incl. this header
    std::uint32_t crc32;  // zlib-crc32 on the WHOLE packet (header + payload; this member being nulled)
    std::uint32_t uid;    // unique 32-bit value per request-response pair; (uid == 0) only valid for a response
    opcode_t opcode;      // purpose of this packet
};
static_assert(sizeof(header_t) == 17, "size mismatch");
#pragma pack(pop)


#pragma pack(push, 1)
struct payload_channel_setup_t
{
    clientid_t client_id;
    channel_setup_flags_t flags;
};
static_assert(sizeof(payload_channel_setup_t) == 12, "size mismatch");
#pragma pack(pop)


#pragma pack(push, 1)
struct payload_channel_setup_ack_t
{
    clientid_t client_id;
};
static_assert(sizeof(payload_channel_setup_ack_t) == 8, "size mismatch");
#pragma pack(pop)


#pragma pack(push, 1)
struct payload_status_t
{
    std::uint8_t status;  // status_t
};
static_assert(sizeof(payload_status_t) == 1, "size mismatch");
#pragma pack(pop)


// used by op_socks, op_socks_close and op_socks_disconnected
#pragma pack(push, 1)
struct payload_socks_header_t
{
    socksid_t socks_id;
};
static_assert(sizeof(payload_socks_header_t) == 8, "size mismatch");
#pragma pack(pop)


static constexpr std::size_t max_payload_size = max_packet_size - sizeof(header_t);


template <typename T>
inline constexpr T host2net(T value) noexcept
{ return cix::native_to_little(value); }

template <typename T>
inline constexpr T net2host(T value) noexcept
{ return cix::little_to_native(value); }


std::uint32_t generate_uid() noexcept;
clientid_t generate_client_id() noexcept;
std::uint32_t crc32(const proto::header_t& header) noexcept;

error_t extract_next_packet(
    bytes_t& stream,
    bytes_t& out_packet,
    std::uint32_t* out_uid=nullptr) noexcept;

bytes_t make_channel_setup(clientid_t client_id, channel_setup_flags_t flags);
bytes_t make_channel_setup_ack(std::uint32_t uid, clientid_t client_id);
bytes_t make_status(std::uint32_t uid, status_t status);
bytes_t make_ping();
bytes_t make_socks(socksid_t socks_id, const bytes_t& socks_packet);
bytes_t make_socks_close(socksid_t socks_id);
bytes_t make_socks_disconnected(socksid_t socks_id);
bytes_t make_uninstall_self();

}  // namespace proto
