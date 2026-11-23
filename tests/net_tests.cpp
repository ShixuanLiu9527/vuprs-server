#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "asio.hpp"
#include "server/net/asio_server.h"
#include "server/net/frame.h"
#include "tools/mock_control_service.h"

static int g_failures = 0;

#define CHECK(condition)                                                         \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #condition << std::endl; \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

namespace
{
    std::array<asio::const_buffer, 3> Buffers(const vuprs::net::OutFrame &frame)
    {
        std::array<asio::const_buffer, 3> buffers{
            asio::const_buffer(frame.header.data(), frame.header.size()),
            asio::const_buffer(),
            asio::const_buffer()};
        if (frame.bytes)
        {
            buffers[1] = asio::buffer(*frame.bytes);
        }
        else
        {
            buffers[1] = asio::const_buffer(frame.data_header.data(), frame.data_header.size());
            if (frame.bf_samples)
                buffers[2] = asio::buffer(frame.bf_samples->data(), frame.bf_samples->size() * sizeof(uint32_t));
            else if (frame.scan_values)
                buffers[2] = asio::buffer(frame.scan_values->data(), frame.scan_values->size() * sizeof(uint16_t));
        }
        return buffers;
    }

    bool SendFrame(asio::ip::tcp::socket &socket, const vuprs::net::OutFrame &frame, asio::error_code &ec)
    {
        auto buffers = Buffers(frame);
        asio::write(socket, buffers, ec);
        return !ec;
    }

    bool ReadFrame(asio::ip::tcp::socket &socket,
                   vuprs::net::FrameHeader *header,
                   std::vector<uint8_t> *payload,
                   asio::error_code &ec)
    {
        std::array<uint8_t, vuprs::net::FRAME_HEADER_SIZE> header_bytes{};
        asio::read(socket, asio::buffer(header_bytes), ec);
        if (ec)
            return false;
        std::string error;
        if (!vuprs::net::DecodeFrameHeader(header_bytes.data(), header_bytes.size(), header, &error))
        {
            std::cerr << "decode header failed: " << error << std::endl;
            return false;
        }
        payload->assign(header->payload_bytes, 0);
        if (!payload->empty())
        {
            asio::read(socket, asio::buffer(*payload), ec);
            if (ec)
                return false;
        }
        return true;
    }

    void TestFrameCodec()
    {
        using namespace vuprs::net;
        FrameHeader header;
        header.type = MessageType::BF_DATA;
        header.flags = 0xA5A5;
        header.payload_bytes = 1234;
        header.request_id = 7;
        header.sequence = 0x1122334455667788ULL;
        header.timestamp_ns = 0x99AABBCCDDEEFF00ULL;
        auto bytes = EncodeFrameHeader(header);
        FrameHeader decoded;
        std::string error;
        CHECK(DecodeFrameHeader(bytes.data(), bytes.size(), &decoded, &error));
        CHECK(decoded.type == header.type);
        CHECK(decoded.flags == header.flags);
        CHECK(decoded.payload_bytes == header.payload_bytes);
        CHECK(decoded.request_id == header.request_id);
        CHECK(decoded.sequence == header.sequence);
        CHECK(decoded.timestamp_ns == header.timestamp_ns);

        auto bad = bytes;
        bad[0] = 'X';
        CHECK(!DecodeFrameHeader(bad.data(), bad.size(), &decoded, &error));

        bad = bytes;
        bad[4] = 9;
        CHECK(!DecodeFrameHeader(bad.data(), bad.size(), &decoded, &error));

        BfDataHeader bf;
        bf.sample_rate_millihz = 10000000;
        bf.sample_count = 8192;
        bf.inference_identity = -1;
        bf.inference_valid = 0;
        auto bf_bytes = EncodeBfDataHeader(bf);
        BfDataHeader bf_decoded;
        CHECK(DecodeBfDataHeader(bf_bytes.data(), bf_bytes.size(), &bf_decoded, &error));
        CHECK(bf_decoded.sample_rate_millihz == bf.sample_rate_millihz);
        CHECK(bf_decoded.sample_count == bf.sample_count);
        CHECK(bf_decoded.inference_identity == bf.inference_identity);

        ScanDataHeader scan;
        scan.points_half = 100;
        scan.data_points = 75;
        scan.sample_rate_millihz = 10000000;
        scan.alt_min_mdeg = 15000;
        scan.min_power_mdb = -30000;
        scan.max_power_mdb = 12000;
        auto scan_bytes = EncodeScanDataHeader(scan);
        ScanDataHeader scan_decoded;
        CHECK(DecodeScanDataHeader(scan_bytes.data(), scan_bytes.size(), &scan_decoded, &error));
        CHECK(scan_decoded.points_half == scan.points_half);
        CHECK(scan_decoded.data_points == scan.data_points);
        CHECK(scan_decoded.alt_min_mdeg == scan.alt_min_mdeg);
        CHECK(scan_decoded.min_power_mdb == scan.min_power_mdb);
        CHECK(scan_decoded.max_power_mdb == scan.max_power_mdb);

        auto samples = std::make_shared<std::vector<uint32_t>>(4, 0x12345678U);
        auto bf_frame = MakeBfFrame(bf, samples, 11, NowNs());
        CHECK(bf_frame.payload_bytes == DATA_HEADER_SIZE + 4 * sizeof(uint32_t));
        FrameHeader frame_header;
        CHECK(DecodeFrameHeader(bf_frame.header.data(), bf_frame.header.size(), &frame_header, &error));
        CHECK(frame_header.type == MessageType::BF_DATA);
        CHECK(frame_header.sequence == 11);

        auto values = std::make_shared<std::vector<uint16_t>>(75, 0x1234);
        auto scan_frame = MakeScanFrame(scan, values, 12, NowNs());
        CHECK(scan_frame.payload_bytes == DATA_HEADER_SIZE + 75 * sizeof(uint16_t));
        CHECK(DecodeFrameHeader(scan_frame.header.data(), scan_frame.header.size(), &frame_header, &error));
        CHECK(frame_header.type == MessageType::SCAN_DATA);
    }

    void TestLoopback()
    {
        auto service = std::make_shared<vuprs::mock::MockControlService>();
        vuprs::net::AsioServerConfig config;
        config.bind_address = "127.0.0.1";
        config.port = 0;
        vuprs::net::AsioServer server(config, service);
        std::string error;
        if (!server.Start(&error))
        {
            std::cerr << "server start failed: " << error << std::endl;
            ++g_failures;
            return;
        }
        std::thread io_thread([&]()
                              { server.Io().run(); });

        asio::io_context client_io;
        asio::ip::tcp::socket client(client_io);
        asio::error_code ec;
        const auto endpoint = asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), server.BoundPort());
        client.connect(endpoint, ec);
        if (ec)
        {
            std::cerr << "connect failed: " << ec.message() << std::endl;
            ++g_failures;
            asio::post(server.Io(), [&]()
                       { server.Stop(); });
            io_thread.join();
            return;
        }
        client.set_option(asio::ip::tcp::no_delay(true), ec);

        auto send_request = [&](uint32_t id, const std::string &json)
        {
            auto frame = vuprs::net::MakeJsonFrame(vuprs::net::MessageType::CONTROL_REQUEST, id, json);
            CHECK(SendFrame(client, frame, ec));
        };
        auto read_one = [&](vuprs::net::FrameHeader *header, std::vector<uint8_t> *payload)
        {
            return ReadFrame(client, header, payload, ec);
        };

        send_request(1, R"({"cmd":"ack","params":{}})");
        vuprs::net::FrameHeader header;
        std::vector<uint8_t> payload;
        CHECK(read_one(&header, &payload));
        CHECK(header.type == vuprs::net::MessageType::CONTROL_RESPONSE);
        CHECK(header.request_id == 1);
        {
            auto response = nlohmann::json::parse(std::string(payload.begin(), payload.end()));
            CHECK(response["ok"].get<bool>());
            CHECK(response["cmd"].get<std::string>() == "ack");
            CHECK(response["params"]["protocol_version"].get<std::string>() == "1");
        }

        send_request(2, R"({"cmd":"start","params":{}})");
        CHECK(read_one(&header, &payload));
        CHECK(header.request_id == 2);
        CHECK(nlohmann::json::parse(std::string(payload.begin(), payload.end()))["ok"].get<bool>());

        for (int i = 0; i < 50 && !server.Publisher()->HasSession(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK(server.Publisher()->HasSession());

        vuprs::net::BfDataHeader bf;
        bf.sample_rate_millihz = 10000000;
        bf.sample_count = 4;
        bf.inference_identity = 9;
        bf.inference_valid = 1;
        auto samples = std::make_shared<std::vector<uint32_t>>(4);
        (*samples)[0] = 0x00010000U;
        (*samples)[1] = 0xFFFFFFFFU;
        (*samples)[2] = 0x00000000U;
        (*samples)[3] = 0x00020000U;
        server.Publisher()->Publish(vuprs::net::MakeBfFrame(bf, samples, 100, vuprs::net::NowNs()));

        CHECK(read_one(&header, &payload));
        CHECK(header.type == vuprs::net::MessageType::BF_DATA);
        CHECK(header.sequence == 100);
        CHECK(payload.size() == vuprs::net::DATA_HEADER_SIZE + 4 * sizeof(uint32_t));
        vuprs::net::BfDataHeader bf_out;
        std::string decode_error;
        CHECK(vuprs::net::DecodeBfDataHeader(payload.data(), payload.size(), &bf_out, &decode_error));
        CHECK(bf_out.sample_count == 4);
        CHECK(bf_out.inference_identity == 9);

        vuprs::net::ScanDataHeader scan;
        scan.points_half = 100;
        scan.data_points = 3;
        scan.sample_rate_millihz = 10000000;
        scan.alt_min_mdeg = 15000;
        scan.min_power_mdb = -30000;
        scan.max_power_mdb = 12000;
        auto values = std::make_shared<std::vector<uint16_t>>(3);
        (*values)[0] = 0;
        (*values)[1] = 32768;
        (*values)[2] = 65535;
        server.Publisher()->Publish(vuprs::net::MakeScanFrame(scan, values, 101, vuprs::net::NowNs()));

        CHECK(read_one(&header, &payload));
        CHECK(header.type == vuprs::net::MessageType::SCAN_DATA);
        CHECK(header.sequence == 101);
        CHECK(payload.size() == vuprs::net::DATA_HEADER_SIZE + 3 * sizeof(uint16_t));
        vuprs::net::ScanDataHeader scan_out;
        CHECK(vuprs::net::DecodeScanDataHeader(payload.data(), payload.size(), &scan_out, &decode_error));
        CHECK(scan_out.points_half == 100);
        CHECK(scan_out.data_points == 3);

        send_request(3, R"({"cmd":"stop","params":{}})");
        CHECK(read_one(&header, &payload));
        CHECK(header.request_id == 3);
        CHECK(nlohmann::json::parse(std::string(payload.begin(), payload.end()))["ok"].get<bool>());

        client.close(ec);
        asio::post(server.Io(), [&]()
                   { server.Stop(); });
        io_thread.join();
    }
}

int main()
{
    TestFrameCodec();
    TestLoopback();
    if (g_failures == 0)
    {
        std::cout << "all network tests passed." << std::endl;
        return 0;
    }
    std::cerr << g_failures << " test(s) failed." << std::endl;
    return 1;
}
