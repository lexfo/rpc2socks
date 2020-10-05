// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"

namespace proto {

namespace detail
{
    // static std::mt19937 rand_gen32((std::random_device())());
    // static std::mt19937_64 rand_gen64((std::random_device())());
    // static std::uniform_int_distribution<std::uint32_t> rand_dist32;
    // static std::uniform_int_distribution<std::uint64_t> rand_dist64;
    static cix::random::fast rand_gen;
    static std::mutex rand_mutex;


    static error_t extract_packet(
        bytes_t& out_packet, std::uint32_t* out_uid,
        const proto::header_t& header, std::size_t remaining_size) noexcept
    {
        const auto declared_len =
            static_cast<std::size_t>(net2host(header.len));

        if (out_uid)
            *out_uid = net2host(header.uid);

        // can header.len be considered "safe"?
        if (declared_len > proto::max_packet_size)
            return error_toobig;

        // enough data for the whole packet?
        if (declared_len > remaining_size)
            return error_incomplete;

        // crc32
        const auto crc = proto::crc32(header);
        if (crc != net2host(header.crc32))
            return error_crc;

        // copy bytes
        out_packet.clear();
        out_packet.reserve(declared_len);
        std::copy(
            reinterpret_cast<const proto::byte_t*>(&header),
            reinterpret_cast<const proto::byte_t*>(&header) + declared_len,
            std::back_inserter(out_packet));

        // convert header
        auto out_header = reinterpret_cast<proto::header_t*>(out_packet.data());
        out_header->len = proto::net2host(out_header->len);
        out_header->crc32 = proto::net2host(out_header->crc32);
        out_header->uid = proto::net2host(out_header->uid);
        out_header->opcode = proto::net2host(out_header->opcode);

        // match packet length against expected size and convert payload
        switch (out_header->opcode)
        {
            case op_channel_setup:
            {
                if (out_header->len !=
                        sizeof(header_t) +
                        sizeof(payload_channel_setup_t))
                {
                    return error_malformed;
                }

                auto payload = reinterpret_cast<payload_channel_setup_t*>(
                    out_packet.data() + sizeof(header_t));

                payload->client_id = proto::net2host(payload->client_id);
                payload->flags = proto::net2host(payload->flags);
                break;
            }

            case op_channel_setup_ack:
            {
                if (out_header->len !=
                        sizeof(header_t) +
                        sizeof(payload_channel_setup_ack_t))
                {
                    return error_malformed;
                }

                auto payload = reinterpret_cast<payload_channel_setup_ack_t*>(
                    out_packet.data() + sizeof(header_t));

                payload->client_id = proto::net2host(payload->client_id);
                break;
            }

            case op_status:
            {
                if (out_header->len !=
                        sizeof(header_t) +
                        sizeof(payload_status_t))
                {
                    return error_malformed;
                }

                auto payload = reinterpret_cast<payload_status_t*>(
                    out_packet.data() + sizeof(header_t));

                payload->status = proto::net2host(payload->status);
                break;
            }

            case op_ping:
            case op_uninstall_self:
                if (out_header->len != sizeof(proto::header_t))
                    return error_malformed;
                break;

            case op_socks:
            {
                if (out_header->len <
                    sizeof(header_t) +
                    sizeof(payload_socks_header_t) +
                    1)
                {
                    return error_malformed;
                }

                auto payload = reinterpret_cast<payload_socks_header_t*>(
                    out_packet.data() + sizeof(header_t));

                payload->socks_id = proto::net2host(payload->socks_id);
                break;
            }

            case op_socks_close:
            case op_socks_disconnected:
            {
                if (out_header->len !=
                    sizeof(header_t) +
                    sizeof(payload_socks_header_t))
                {
                    return error_malformed;
                }

                auto payload = reinterpret_cast<payload_socks_header_t*>(
                    out_packet.data() + sizeof(header_t));

                payload->socks_id = proto::net2host(payload->socks_id);
                break;
            }

            default:
                assert(0);  // opcode should be taken into account
                break;
        }

        return proto::ok;
    }


    static bytes_t make_packet(
        std::uint32_t uid, proto::opcode_t opcode, std::size_t payload_size=0)
    {
        const auto packet_len = sizeof(header_t) + payload_size;

        if (packet_len > proto::max_packet_size ||
            packet_len > std::numeric_limits<decltype(header_t::len)>::max())
        {
            CIX_THROW_LENGTH("new packet too big");
        }

        bytes_t out(packet_len);

        auto header = reinterpret_cast<header_t*>(out.data());

        std::memcpy(&header->magic, &proto::magic, sizeof(header->magic));
        header->len = host2net(static_cast<decltype(header_t::len)>(packet_len));
        header->uid = host2net(
            (uid != std::numeric_limits<std::uint32_t>::max()) ? uid :
            proto::generate_uid());
        header->opcode = host2net(opcode);

        return out;
    }


    static void consolidate_packet(bytes_t& packet)
    {
        if (packet.size() > std::numeric_limits<decltype(header_t::len)>::max())
            CIX_THROW_LENGTH("new packet too big");

        auto header = reinterpret_cast<header_t*>(packet.data());

        assert(header->opcode);

        header->len = static_cast<decltype(header_t::len)>(packet.size());
        header->crc32 = proto::crc32(*header);
    }
}


std::uint32_t generate_uid() noexcept
{
    std::scoped_lock lock(detail::rand_mutex);

    for (;;)
    {
        const auto now = static_cast<uint32_t>(cix::ticks_now());
        // const auto shuffle = detail::rand_dist32(detail::rand_gen32);
        const auto shuffle = detail::rand_gen.next32();
        const auto uid = (now & 0x0000f0ff) | (shuffle & 0xffff0f00);

        if (uid != 0)
            return uid;
    }
}


clientid_t generate_client_id() noexcept
{
    std::scoped_lock lock(detail::rand_mutex);
    proto::clientid_t id;

    // do { id = detail::rand_dist64(detail::rand_gen64); }
    do { id = detail::rand_gen.next64(); }
    while (id == proto::invalid_client_id);

    return id;
}


std::uint32_t crc32(const proto::header_t& header) noexcept
{
    const cix::crc32::hash_t zero32 = 0;

    static_assert(
        sizeof(zero32) == sizeof(proto::header_t::crc32),
        "size mismatch");

    auto crc32ctx = cix::crc32::create();

    std::size_t offset = offsetof(proto::header_t, crc32);

    // up to header.crc32
    cix::crc32::update(crc32ctx, &header, offset);

    // zeroed header.crc32
    cix::crc32::update(crc32ctx, &zero32, sizeof(zero32));

    // header tail + payload
    offset += sizeof(zero32);
    const auto tail_size =
        static_cast<std::size_t>(net2host(header.len)) -
        offset;
    cix::crc32::update(
        crc32ctx,
        reinterpret_cast<const proto::byte_t*>(&header) + offset,
        tail_size);

    return cix::crc32::finalize(crc32ctx);
}


error_t extract_next_packet(
    bytes_t& stream, bytes_t& out_packet, std::uint32_t* out_uid) noexcept
{
    out_packet.clear();
    if (out_uid)
        *out_uid = 0;

    if (stream.empty())
        return error_incomplete;

    // search for magic word
    auto stream_it = std::search(
        stream.begin(), stream.end(),
        proto::magic.begin(), proto::magic.end());

    // found it?
    if (stream_it == stream.end())
    {
        stream.clear();
        return error_garbage;
    }

    // remaining data size in stream
    // static_cast is ok here since (it < end)
    const auto remaining_size = static_cast<std::size_t>(
        std::distance(stream_it, stream.end()));

    // enough data for the header?
    if (remaining_size < sizeof(proto::header_t))
    {
        stream.erase(stream.begin(), stream_it);
        return error_incomplete;
    }

    const auto& header = *reinterpret_cast<const proto::header_t*>(&*stream_it);
    const auto error = detail::extract_packet(
        out_packet, out_uid, header, remaining_size);

    switch (error)
    {
        case proto::ok:
            assert(!out_packet.empty());
            stream.erase(
                stream.begin(),
                std::next(stream_it, out_packet.size()));
            break;

        case error_garbage:
            stream.clear();
            break;

        case error_incomplete:
            stream.erase(stream.begin(), stream_it);
            break;

        case error_malformed:
            stream.erase(
                stream.begin(),
                std::next(stream_it, net2host(header.len)));
            break;

        case error_toobig:
            stream.erase(
                stream.begin(),
                std::next(stream_it, proto::magic.size()));
            break;

        case error_crc:
            stream.erase(
                stream.begin(),
                std::next(stream_it, net2host(header.len)));
            break;

        default:
            assert(0);
            stream.erase(
                stream.begin(),
                std::next(stream_it, proto::magic.size()));
            break;
    }

    return error;
}


bytes_t make_channel_setup(clientid_t client_id, channel_setup_flags_t flags)
{
    auto packet = detail::make_packet(
        generate_uid(),
        proto::op_channel_setup,
        sizeof(payload_channel_setup_t));

    auto payload = reinterpret_cast<payload_channel_setup_t*>(
        packet.data() + sizeof(header_t));

    payload->client_id = host2net(client_id);
    payload->flags = host2net(flags);

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_channel_setup_ack(std::uint32_t uid, clientid_t client_id)
{
    auto packet = detail::make_packet(
        uid,
        proto::op_channel_setup_ack,
        sizeof(payload_channel_setup_ack_t));

    auto payload = reinterpret_cast<payload_channel_setup_ack_t*>(
        packet.data() + sizeof(header_t));

    payload->client_id = host2net(client_id);

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_status(std::uint32_t uid, status_t status)
{
    auto packet = detail::make_packet(
        uid,
        proto::op_status,
        sizeof(payload_status_t));

    auto payload = reinterpret_cast<payload_status_t*>(
        packet.data() + sizeof(header_t));

    payload->status = host2net(status);

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_ping()
{
    auto packet = detail::make_packet(generate_uid(), proto::op_ping);
    detail::consolidate_packet(packet);
    return packet;
}


bytes_t make_socks(socksid_t socks_id, const bytes_t& socks_packet)
{
    if (socks_id == invalid_socks_id)
        CIX_THROW_BADARG("invalid SOCKS connection id");

    if (socks_packet.empty())
        CIX_THROW_BADARG("empty SOCKS packet");

    auto packet = detail::make_packet(
        generate_uid(),
        proto::op_socks,
        sizeof(payload_socks_header_t) + socks_packet.size());

    auto payload_header = reinterpret_cast<payload_socks_header_t*>(
        packet.data() + sizeof(header_t));

    payload_header->socks_id = host2net(socks_id);

    // SOCKS packet
    std::memcpy(
        packet.data() + sizeof(header_t) + sizeof(payload_socks_header_t),
        socks_packet.data(),
        socks_packet.size());

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_socks_close(socksid_t socks_id)
{
    if (socks_id == invalid_socks_id)
        CIX_THROW_BADARG("invalid SOCKS connection id");

    auto packet = detail::make_packet(
        generate_uid(),
        proto::op_socks_close,
        sizeof(payload_socks_header_t));

    auto payload = reinterpret_cast<payload_socks_header_t*>(
        packet.data() + sizeof(header_t));

    payload->socks_id = host2net(socks_id);

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_socks_disconnected(socksid_t socks_id)
{
    if (socks_id == invalid_socks_id)
        CIX_THROW_BADARG("invalid SOCKS connection id");

    auto packet = detail::make_packet(
        generate_uid(),
        proto::op_socks_disconnected,
        sizeof(payload_socks_header_t));

    auto payload = reinterpret_cast<payload_socks_header_t*>(
        packet.data() + sizeof(header_t));

    payload->socks_id = host2net(socks_id);

    detail::consolidate_packet(packet);

    return packet;
}


bytes_t make_uninstall_self()
{
    auto packet = detail::make_packet(generate_uid(), proto::op_uninstall_self);
    detail::consolidate_packet(packet);
    return packet;
}


}  // namespace proto
