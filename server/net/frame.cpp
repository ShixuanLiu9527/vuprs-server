#include "server/net/frame.h"

#include <chrono>
#include <cstring>

namespace
{
    void PutU16(uint8_t *p, uint16_t value)
    {
        p[0] = static_cast<uint8_t>(value & 0xFF);
        p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    void PutU32(uint8_t *p, uint32_t value)
    {
        p[0] = static_cast<uint8_t>(value & 0xFF);
        p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }

    void PutU64(uint8_t *p, uint64_t value)
    {
        for (int i = 0; i < 8; i++)
            p[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }

    uint16_t GetU16(const uint8_t *p)
    {
        return static_cast<uint16_t>(p[0]) |
               static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
    }

    uint32_t GetU32(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    uint64_t GetU64(const uint8_t *p)
    {
        uint64_t value = 0;
        for (int i = 7; i >= 0; i--)
            value = (value << 8) | static_cast<uint64_t>(p[i]);
        return value;
    }

    bool IsKnownType(vuprs::net::MessageType type)
    {
        switch (type)
        {
        case vuprs::net::MessageType::CONTROL_REQUEST:
        case vuprs::net::MessageType::CONTROL_RESPONSE:
        case vuprs::net::MessageType::BF_DATA:
        case vuprs::net::MessageType::SCAN_DATA:
            return true;
        default:
            return false;
        }
    }

    void SetError(std::string *error, const std::string &message)
    {
        if (error != nullptr)
            *error = message;
    }
}

uint64_t vuprs::net::NowNs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string vuprs::net::MessageTypeName(MessageType type)
{
    switch (type)
    {
    case MessageType::CONTROL_REQUEST:
        return "control_request";
    case MessageType::CONTROL_RESPONSE:
        return "control_response";
    case MessageType::BF_DATA:
        return "bf_data";
    case MessageType::SCAN_DATA:
        return "scan_data";
    default:
        return "unknown";
    }
}

std::array<uint8_t, vuprs::net::FRAME_HEADER_SIZE> vuprs::net::EncodeFrameHeader(const FrameHeader &header)
{
    std::array<uint8_t, FRAME_HEADER_SIZE> out{};
    std::memcpy(out.data(), FRAME_MAGIC.data(), FRAME_MAGIC.size());
    PutU16(out.data() + 4, header.version);
    PutU16(out.data() + 6, static_cast<uint16_t>(header.type));
    PutU32(out.data() + 8, header.flags);
    PutU32(out.data() + 12, header.payload_bytes);
    PutU32(out.data() + 16, header.request_id);
    PutU32(out.data() + 20, 0);
    PutU64(out.data() + 24, header.sequence);
    PutU64(out.data() + 32, header.timestamp_ns);
    return out;
}

bool vuprs::net::DecodeFrameHeader(const uint8_t *data, size_t bytes, FrameHeader *header, std::string *error)
{
    if (data == nullptr || header == nullptr || bytes < FRAME_HEADER_SIZE)
    {
        SetError(error, "short frame header");
        return false;
    }
    if (std::memcmp(data, FRAME_MAGIC.data(), FRAME_MAGIC.size()) != 0)
    {
        SetError(error, "bad frame magic");
        return false;
    }
    header->version = GetU16(data + 4);
    header->type = static_cast<MessageType>(GetU16(data + 6));
    header->flags = GetU32(data + 8);
    header->payload_bytes = GetU32(data + 12);
    header->request_id = GetU32(data + 16);
    header->sequence = GetU64(data + 24);
    header->timestamp_ns = GetU64(data + 32);
    if (header->version != PROTOCOL_VERSION)
    {
        SetError(error, "unsupported protocol version");
        return false;
    }
    if (!IsKnownType(header->type))
    {
        SetError(error, "unknown message type");
        return false;
    }
    if (header->payload_bytes > MAX_PAYLOAD_BYTES)
    {
        SetError(error, "payload too large");
        return false;
    }
    return true;
}

std::array<uint8_t, vuprs::net::DATA_HEADER_SIZE> vuprs::net::EncodeBfDataHeader(const BfDataHeader &header)
{
    std::array<uint8_t, DATA_HEADER_SIZE> out{};
    PutU32(out.data() + 0, header.sample_rate_millihz);
    PutU32(out.data() + 4, header.sample_count);
    PutU32(out.data() + 8, header.sample_format);
    PutU32(out.data() + 12, static_cast<uint32_t>(header.inference_identity));
    PutU32(out.data() + 16, header.inference_valid);
    PutU32(out.data() + 20, header.reserved[0]);
    PutU32(out.data() + 24, header.reserved[1]);
    PutU32(out.data() + 28, header.reserved[2]);
    return out;
}

std::array<uint8_t, vuprs::net::DATA_HEADER_SIZE> vuprs::net::EncodeScanDataHeader(const ScanDataHeader &header)
{
    std::array<uint8_t, DATA_HEADER_SIZE> out{};
    PutU32(out.data() + 0, header.points_half);
    PutU32(out.data() + 4, header.data_points);
    PutU32(out.data() + 8, header.sample_rate_millihz);
    PutU32(out.data() + 12, static_cast<uint32_t>(header.alt_min_mdeg));
    PutU32(out.data() + 16, static_cast<uint32_t>(header.min_power_mdb));
    PutU32(out.data() + 20, static_cast<uint32_t>(header.max_power_mdb));
    PutU32(out.data() + 24, header.scan_format);
    PutU32(out.data() + 28, header.reserved);
    return out;
}

bool vuprs::net::DecodeBfDataHeader(const uint8_t *data, size_t bytes, BfDataHeader *header, std::string *error)
{
    if (data == nullptr || header == nullptr || bytes < DATA_HEADER_SIZE)
    {
        SetError(error, "short BF data header");
        return false;
    }
    header->sample_rate_millihz = GetU32(data + 0);
    header->sample_count = GetU32(data + 4);
    header->sample_format = GetU32(data + 8);
    header->inference_identity = static_cast<int32_t>(GetU32(data + 12));
    header->inference_valid = GetU32(data + 16);
    header->reserved[0] = GetU32(data + 20);
    header->reserved[1] = GetU32(data + 24);
    header->reserved[2] = GetU32(data + 28);
    if (header->sample_format != static_cast<uint32_t>(SampleFormat::INT32_Q16_16))
    {
        SetError(error, "unsupported BF sample format");
        return false;
    }
    return true;
}

bool vuprs::net::DecodeScanDataHeader(const uint8_t *data, size_t bytes, ScanDataHeader *header, std::string *error)
{
    if (data == nullptr || header == nullptr || bytes < DATA_HEADER_SIZE)
    {
        SetError(error, "short scan data header");
        return false;
    }
    header->points_half = GetU32(data + 0);
    header->data_points = GetU32(data + 4);
    header->sample_rate_millihz = GetU32(data + 8);
    header->alt_min_mdeg = static_cast<int32_t>(GetU32(data + 12));
    header->min_power_mdb = static_cast<int32_t>(GetU32(data + 16));
    header->max_power_mdb = static_cast<int32_t>(GetU32(data + 20));
    header->scan_format = GetU32(data + 24);
    header->reserved = GetU32(data + 28);
    if (header->scan_format != static_cast<uint32_t>(ScanFormat::UINT16_MINMAX_NORMALIZED))
    {
        SetError(error, "unsupported scan format");
        return false;
    }
    return true;
}

vuprs::net::OutFrame vuprs::net::MakeJsonFrame(MessageType type, uint32_t request_id, const std::string &payload)
{
    OutFrame frame;
    FrameHeader header;
    header.type = type;
    header.payload_bytes = static_cast<uint32_t>(payload.size());
    header.request_id = request_id;
    header.timestamp_ns = NowNs();
    frame.header = EncodeFrameHeader(header);
    auto bytes = std::make_shared<std::vector<uint8_t>>(payload.begin(), payload.end());
    frame.bytes = std::move(bytes);
    frame.payload_bytes = header.payload_bytes;
    return frame;
}

vuprs::net::OutFrame vuprs::net::MakeBfFrame(const BfDataHeader &data_header,
                                             std::shared_ptr<const std::vector<uint32_t>> samples,
                                             uint64_t sequence,
                                             uint64_t timestamp_ns)
{
    OutFrame frame;
    const size_t sample_count = samples ? samples->size() : 0;
    FrameHeader header;
    header.type = MessageType::BF_DATA;
    header.sequence = sequence;
    header.timestamp_ns = timestamp_ns;
    header.payload_bytes = static_cast<uint32_t>(DATA_HEADER_SIZE + sample_count * sizeof(uint32_t));
    frame.header = EncodeFrameHeader(header);
    frame.data_header = EncodeBfDataHeader(data_header);
    frame.bf_samples = std::move(samples);
    frame.payload_bytes = header.payload_bytes;
    return frame;
}

vuprs::net::OutFrame vuprs::net::MakeScanFrame(const ScanDataHeader &data_header,
                                               std::shared_ptr<const std::vector<uint16_t>> values,
                                               uint64_t sequence,
                                               uint64_t timestamp_ns)
{
    OutFrame frame;
    const size_t data_points = values ? values->size() : 0;
    FrameHeader header;
    header.type = MessageType::SCAN_DATA;
    header.sequence = sequence;
    header.timestamp_ns = timestamp_ns;
    header.payload_bytes = static_cast<uint32_t>(DATA_HEADER_SIZE + data_points * sizeof(uint16_t));
    frame.header = EncodeFrameHeader(header);
    frame.data_header = EncodeScanDataHeader(data_header);
    frame.scan_values = std::move(values);
    frame.payload_bytes = header.payload_bytes;
    return frame;
}
