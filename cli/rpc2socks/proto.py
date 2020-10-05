# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import enum
import random
import struct
import threading
import zlib

from .utils import logging
from .utils import NoDict

MAGIC = b"\xe4\x85\xb4\xb2"
ENDIANNESS = "<"  # little endian

HEADER_STRUCT = struct.Struct(
    ENDIANNESS +
    "4s"  # MAGIC
    "I"   # len; total packet length (bytes), incl. this header
    "I"   # crc32; zlib-crc32 on the WHOLE packet (header + payload; this member being nulled)
    "I"   # uid; unique 32-bit value per request-response pair; (uid == 0) only valid for a response
    "B")  # opcode; purpose of this packet

HEADER_CRC32_OFFSET = 8
MAX_PACKET_SIZE = 16 * 1024 * 1024
MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - HEADER_STRUCT.size
INVALID_SOCKS_ID = 0

logger = logging.get_internal_logger(__name__)


class ProtoError(Exception):
    pass


class ProtoDecodeError(ProtoError):
    pass


@enum.unique
class OpCode(enum.IntEnum):
    CHANNEL_SETUP = 1
    CHANNEL_SETUP_ACK = 2
    STATUS = 5
    PING = 10
    SOCKS = 150               # sent by client or server side
    SOCKS_CLOSE = 151         # sent by client or server side
    SOCKS_DISCONNECTED = 152  # sent by client or server side
    UNINSTALL_SELF = 240


@enum.unique
class Status(enum.IntEnum):
    OK = 0
    UNSUPPORTED = 1


@enum.unique
class ChannelSetupFlag(enum.IntFlag):
    READ = 0x01
    WRITE = 0x02
    DUPLEX = READ | WRITE


class PacketBase:
    def __init__(self, opcode, *, uid=None):
        assert isinstance(opcode, OpCode)
        self.opcode = opcode
        self.uid = uid

    def serialize(self):
        payload = self._serialize_payload()
        packet_len = HEADER_STRUCT.size + len(payload)

        if self.uid is None:
            self.uid = generate_uid()

        header = HEADER_STRUCT.pack(
            MAGIC, packet_len, 0, self.uid, self.opcode.value)

        # compute crc32
        crc32 = zlib.crc32(header)
        if len(payload) > 0:
            crc32 = zlib.crc32(payload, crc32)

        header = HEADER_STRUCT.pack(
            MAGIC, packet_len, crc32, self.uid, self.opcode.value)

        return header + payload

    def _serialize_payload(self):
        return b""

    @classmethod
    def create_from_packet(cls, header, payload_view):
        raise NotImplementedError


class ChannelSetupPacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "QL")

    def __init__(self, client_id, flags, **kwargs):
        validate_client_id(client_id)
        if not isinstance(flags, ChannelSetupFlag):
            raise ValueError("flags")

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.CHANNEL_SETUP, **kwargs)

        self.client_id = client_id
        self.flags = flags

    def _serialize_payload(self):
        validate_client_id(self.client_id)
        if not isinstance(self.flags, ChannelSetupFlag):
            raise ValueError("flags")

        return self.PAYLOAD_STRUCT.pack(
            self.client_id,
            self.flags.value)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected "
                f"{cls.PAYLOAD_STRUCT.size})")

        client_id, flags = cls.PAYLOAD_STRUCT.unpack(payload_view)
        flags = ChannelSetupFlag(flags)

        return cls(client_id, flags, uid=header.uid)


class ChannelSetupAckPacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "Q")

    def __init__(self, client_id, **kwargs):
        validate_client_id(client_id)

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.CHANNEL_SETUP_ACK, **kwargs)

        self.client_id = client_id

    def _serialize_payload(self):
        validate_client_id(self.client_id)

        return self.PAYLOAD_STRUCT.pack(self.client_id)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected "
                f"{cls.PAYLOAD_STRUCT.size})")

        client_id, = cls.PAYLOAD_STRUCT.unpack(payload_view)

        return cls(client_id, uid=header.uid)


class StatusPacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "B")

    def __init__(self, status, **kwargs):
        assert isinstance(status, int)
        assert status in Status.__members__.values()

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.STATUS, **kwargs)

        self.status = Status(status)

    def _serialize_payload(self):
        return self.PAYLOAD_STRUCT.pack(self.status.value)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected "
                f"{cls.PAYLOAD_STRUCT.size})")

        status, = cls.PAYLOAD_STRUCT.unpack(payload_view)

        if status in Status.__members__.values():
            status = Status(status)
        else:
            logger.warning(f"received an unknown status value: {status}")

        return cls(status, uid=header.uid)


class PingPacket(PacketBase):
    def __init__(self, **kwargs):
        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.PING, **kwargs)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != 0:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: expected empty "
                f"payload")

        return cls(uid=header.uid)


class SocksPacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "Q")

    def __init__(self, socks_id, socks_packet, **kwargs):
        validate_socks_id(socks_id)
        if not isinstance(socks_packet, bytes):
            raise ValueError("socks_packet")

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.SOCKS, **kwargs)

        self.socks_id = socks_id
        self.socks_packet = socks_packet

    def _serialize_payload(self):
        validate_socks_id(self.socks_id)
        if not isinstance(self.socks_packet, bytes):
            raise ValueError("socks_packet")

        payload = self.PAYLOAD_STRUCT.pack(self.socks_id)
        payload += self.socks_packet
        return payload

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) <= cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected more than "
                f"{cls.PAYLOAD_STRUCT.size})")

        head_view = payload_view[0:cls.PAYLOAD_STRUCT.size]
        tail_view = payload_view[cls.PAYLOAD_STRUCT.size:]

        socks_id, = cls.PAYLOAD_STRUCT.unpack(head_view)
        socks_packet = tail_view.tobytes()

        return cls(socks_id, socks_packet, uid=header.uid)


class SocksClosePacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "Q")

    def __init__(self, socks_id, **kwargs):
        validate_socks_id(socks_id)

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.SOCKS_CLOSE, **kwargs)

        self.socks_id = socks_id

    def _serialize_payload(self):
        validate_socks_id(self.socks_id)

        return self.PAYLOAD_STRUCT.pack(self.socks_id)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected "
                f"{cls.PAYLOAD_STRUCT.size})")

        socks_id, = cls.PAYLOAD_STRUCT.unpack(payload_view)

        return cls(socks_id, uid=header.uid)


class SocksDisconnectedPacket(PacketBase):
    PAYLOAD_STRUCT = struct.Struct(ENDIANNESS + "Q")

    def __init__(self, socks_id, **kwargs):
        validate_socks_id(socks_id)

        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.SOCKS_DISCONNECTED, **kwargs)

        self.socks_id = socks_id

    def _serialize_payload(self):
        validate_socks_id(self.socks_id)

        return self.PAYLOAD_STRUCT.pack(self.socks_id)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != cls.PAYLOAD_STRUCT.size:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: unexpected payload "
                f"length (got {len(payload_view)}; expected "
                f"{cls.PAYLOAD_STRUCT.size})")

        socks_id, = cls.PAYLOAD_STRUCT.unpack(payload_view)

        return cls(socks_id, uid=header.uid)


class UninstallSelfPacket(PacketBase):
    __slots__ = ()

    def __init__(self, **kwargs):
        kwargs.setdefault("uid", generate_uid())
        super().__init__(OpCode.UNINSTALL_SELF, **kwargs)

    @classmethod
    def create_from_packet(cls, header, payload_view):
        if len(payload_view) != 0:
            raise ProtoDecodeError(
                f"malformed {header.opcode.name} packet: expected empty "
                f"payload")

        return cls(uid=header.uid)


class _ExtractedHeader(NoDict):
    __slots__ = ("packet_len", "uid", "opcode")

    def __init__(self, packet_len, uid, opcode):
        assert isinstance(packet_len, int)
        assert isinstance(uid, int)
        assert isinstance(opcode, int)
        assert opcode in OpCode.__members__.values()

        self.packet_len = packet_len
        self.uid = uid
        self.opcode = OpCode(opcode)


class InputStream:
    def __init__(self):
        self.feed_lock = threading.Lock()
        self.input_queue = []
        self.input_buffer = b""
        self.flush_lock = threading.Lock()

    def __bool__(self):
        with self.feed_lock:
            return self.input_queue or self.input_buffer

    def __len__(self):
        with self.feed_lock:
            size = len(self.input_buffer)
            for data in self.input_queue:
                size += len(data)

            return size

    def clear(self):
        with self.flush_lock:
            with self.feed_lock:
                self.input_queue = []
                self.input_buffer = b""

    def feed(self, data):
        with self.feed_lock:
            if isinstance(data, list):
                # if __debug__:
                for dat in data:
                    if not isinstance(dat, bytes):
                        raise ValueError(f"unsupported data type: {type(data)}")
                self.input_queue.extend(data)
            elif isinstance(data, bytes):
                self.input_queue.append(data)
            else:
                raise ValueError(f"unsupported data type: {type(data)}")

    def flush_next_packet(self):
        with self.flush_lock:
            with self.feed_lock:
                if self.input_queue:
                    self.input_buffer += b"".join(self.input_queue)
                    self.input_queue = []

                if not self.input_buffer:
                    return None

                # find magic word
                offset = self.input_buffer.find(MAGIC)
                if offset < 0:
                    logger.warning(
                        f"skipping {len(self.input_buffer)} bytes of garbage "
                        f"input data")

                    self.input_buffer = b""
                    return None
                elif offset > 0:
                    logger.warning(
                        f"skipping {offset} bytes of garbage input data")

                    self.input_buffer = self.input_buffer[offset:]
                    if not self.input_buffer:
                        return None

                input_buffer_copy = self.input_buffer[:]  # copy

            view = memoryview(input_buffer_copy)

            header = self._read_header(view)
            if header is None:
                return None

            packet = self._read_payload(
                header, view[HEADER_STRUCT.size:header.packet_len])
            if packet is None:
                return None

            # consume buffer data
            with self.feed_lock:
                assert len(self.input_buffer) >= header.packet_len
                self.input_buffer = self.input_buffer[header.packet_len:]

        return packet

    def _read_header(self, view):
        # enough data for the header?
        if len(view) < HEADER_STRUCT.size:
            return None

        magic, packet_len, crc32, uid, opcode = \
            HEADER_STRUCT.unpack(view[0:HEADER_STRUCT.size])

        # validate magic word
        if magic != MAGIC:
            raise ProtoDecodeError(
                f"malformed packet: incorrect magic word {repr(magic)}")

        # sanity check (packet_len)
        if packet_len > MAX_PACKET_SIZE:
            raise ProtoDecodeError(
                f"malformed packet: header.packet_len is {packet_len}")

        # validate opcode
        if opcode not in OpCode.__members__.values():
            raise ProtoDecodeError(
                f"malformed packet: unknown opcode {opcode}")

        # enough data for the whole packet?
        if len(view) < packet_len:
            return None

        # validate crc32
        actual_crc32 = crc32_packet(view[0:packet_len])
        if actual_crc32 != crc32:
            raise ProtoDecodeError(
                f"malformed packet: crc32 mismatch "
                f"(got {actual_crc32}; expected {crc32}")

        return _ExtractedHeader(packet_len, uid, opcode)

    def _read_payload(self, header, payload_view):
        # convert opcode name to packet class name
        # e.g. "CHANNEL_SETUP" to "ChannelSetupPacket"
        class_name = header.opcode.name
        class_name = class_name.replace("_", " ")
        class_name = class_name.title()
        class_name = class_name.replace(" ", "")
        class_name += "Packet"

        try:
            klass = globals()[class_name]
        except KeyError:
            raise ProtoDecodeError(
                f"packet class \"{class_name}\" not found for proto opcode "
                f"#{header.opcode}")

        klass = globals()[class_name]
        method = getattr(klass, "create_from_packet")

        return method(header, payload_view)


def generate_uid():
    return random.randint(1, 0xffff_fffe)


def generate_client_id():
    while True:
        client_id = random.randint(1, 0xffff_ffff_ffff_fffe)

        # value 0 is used in ChannelSetupPacket by the client side
        # to indicate it does not have an ID yet
        if client_id != 0:
            return client_id


def generate_socks_id():
    while True:
        socks_id = random.randint(1, 0xffff_ffff_ffff_fffe)
        if socks_id != INVALID_SOCKS_ID:
            return socks_id


def validate_client_id(client_id):
    if (not isinstance(client_id, int) or
            client_id < 0 or
            client_id > 0xffff_ffff_ffff_ffff):
        raise ValueError("client_id")


def validate_socks_id(socks_id):
    if (not isinstance(socks_id, int) or
            socks_id < 0 or
            socks_id > 0xffff_ffff_ffff_ffff or
            socks_id == INVALID_SOCKS_ID):
        raise ValueError("socks_id")


def crc32_packet(data):
    crc32 = zlib.crc32(data[0:HEADER_CRC32_OFFSET])
    crc32 = zlib.crc32(b"\x00\x00\x00\x00", crc32)
    crc32 = zlib.crc32(data[HEADER_CRC32_OFFSET+4:], crc32)
    return crc32


def bytes_to_hexstr(data):
    return f"[{len(data)} bytes]: " + " ".join("{:02X}".format(b) for b in data)
