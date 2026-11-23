#ifndef VUPRS_NET_FRAME_H
#define VUPRS_NET_FRAME_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vuprs
{
    namespace net
    {
        constexpr size_t FRAME_HEADER_SIZE = 40;
        constexpr size_t DATA_HEADER_SIZE = 32;
        constexpr size_t MAX_PAYLOAD_BYTES = 8UL * 1024UL * 1024UL;
        constexpr uint16_t PROTOCOL_VERSION = 1;

        constexpr std::array<uint8_t, 4> FRAME_MAGIC = {'V', 'U', 'P', 'R'};

        enum class MessageType : uint16_t
        {
            CONTROL_REQUEST = 0x0001,
            CONTROL_RESPONSE = 0x0002,
            BF_DATA = 0x0003,
            SCAN_DATA = 0x0004
        };

        enum class SampleFormat : uint32_t
        {
            INT32_Q16_16 = 1
        };

        enum class ScanFormat : uint32_t
        {
            UINT16_MINMAX_NORMALIZED = 1
        };

        struct FrameHeader
        {
            uint16_t version = PROTOCOL_VERSION;
            MessageType type = MessageType::CONTROL_REQUEST;
            uint32_t flags = 0;
            uint32_t payload_bytes = 0;
            uint32_t request_id = 0;
            uint64_t sequence = 0;
            uint64_t timestamp_ns = 0;
        };

        struct BfDataHeader
        {
            uint32_t sample_rate_millihz = 0;
            uint32_t sample_count = 0;
            uint32_t sample_format = static_cast<uint32_t>(SampleFormat::INT32_Q16_16);
            int32_t inference_identity = -1;
            uint32_t inference_valid = 0;
            uint32_t reserved[3] = {0, 0, 0};
        };

        struct ScanDataHeader
        {
            uint32_t points_half = 0;
            uint32_t data_points = 0;
            uint32_t sample_rate_millihz = 0;
            int32_t alt_min_mdeg = 0;
            int32_t min_power_mdb = 0;
            int32_t max_power_mdb = 0;
            uint32_t scan_format = static_cast<uint32_t>(ScanFormat::UINT16_MINMAX_NORMALIZED);
            uint32_t reserved = 0;
        };

        /**
         * @brief Outbound frame. Exactly one payload source is used:
         *        - bytes: complete payload (JSON/control or prebuilt bytes)
         *        - bf_samples: BF samples; data_header is sent before them
         *        - scan_values: scan values; data_header is sent before them
         */
        struct OutFrame
        {
            std::array<uint8_t, FRAME_HEADER_SIZE> header{};
            std::array<uint8_t, DATA_HEADER_SIZE> data_header{};
            std::shared_ptr<const std::vector<uint8_t>> bytes;
            std::shared_ptr<const std::vector<uint32_t>> bf_samples;
            std::shared_ptr<const std::vector<uint16_t>> scan_values;
            uint32_t payload_bytes = 0;
        };

        uint64_t NowNs();
        std::string MessageTypeName(MessageType type);

        std::array<uint8_t, FRAME_HEADER_SIZE> EncodeFrameHeader(const FrameHeader &header);
        bool DecodeFrameHeader(const uint8_t *data, size_t bytes, FrameHeader *header, std::string *error);

        std::array<uint8_t, DATA_HEADER_SIZE> EncodeBfDataHeader(const BfDataHeader &header);
        std::array<uint8_t, DATA_HEADER_SIZE> EncodeScanDataHeader(const ScanDataHeader &header);
        bool DecodeBfDataHeader(const uint8_t *data, size_t bytes, BfDataHeader *header, std::string *error);
        bool DecodeScanDataHeader(const uint8_t *data, size_t bytes, ScanDataHeader *header, std::string *error);

        OutFrame MakeJsonFrame(MessageType type, uint32_t request_id, const std::string &payload);
        OutFrame MakeBfFrame(const BfDataHeader &data_header,
                             std::shared_ptr<const std::vector<uint32_t>> samples,
                             uint64_t sequence,
                             uint64_t timestamp_ns);
        OutFrame MakeScanFrame(const ScanDataHeader &data_header,
                               std::shared_ptr<const std::vector<uint16_t>> values,
                               uint64_t sequence,
                               uint64_t timestamp_ns);
    }
}

#endif
