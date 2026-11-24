#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "server/net/asio_server.h"
#include "server/net/frame.h"
#include "server/test/mock_control_service.h"

static std::atomic<bool> g_running{true};

static void OnSignal(int)
{
    g_running = false;
}

static void PrintUsage(const char *program)
{
    std::cout << "Usage: " << program << " [--bind 127.0.0.1] [--port 18080] [--rate 2]\n";
}

int main(int argc, char *argv[])
{
    std::string bind_address = "127.0.0.1";
    uint16_t port = 18080;
    int rate = 2;
    try
    {
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                PrintUsage(argv[0]);
                return 0;
            }
            if (arg == "--bind" && i + 1 < argc)
                bind_address = argv[++i];
            else if (arg == "--port" && i + 1 < argc)
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            else if (arg == "--rate" && i + 1 < argc)
                rate = std::stoi(argv[++i]);
            else
            {
                PrintUsage(argv[0]);
                return 1;
            }
        }
    }
    catch (...)
    {
        PrintUsage(argv[0]);
        return 1;
    }
    if (rate <= 0)
        rate = 1;

    auto service = std::make_shared<vuprs::mock::MockControlService>();
    vuprs::net::AsioServerConfig config;
    config.bind_address = bind_address;
    config.port = port;
    vuprs::net::AsioServer server(config, service);
    std::string error;
    if (!server.Start(&error))
    {
        std::cerr << "[mock] " << error << std::endl;
        return 1;
    }

    std::cout << "[mock] listening on " << bind_address << ":" << server.BoundPort() << std::endl;
    std::cout << "[mock] protocol 1; control commands + synthetic BF/scan data" << std::endl;

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    auto publisher = server.Publisher();
    std::atomic<bool> generator_running{true};
    std::thread generator([&]()
    {
        const size_t sample_count = 8192;
        auto samples = std::make_shared<std::vector<uint32_t>>(sample_count);
        for (size_t i = 0; i < sample_count; i++)
        {
            const double value = std::sin(2.0 * 3.14159265358979323846 * static_cast<double>(i) / 256.0) * 1000.0 * 65536.0;
            (*samples)[i] = static_cast<uint32_t>(static_cast<int32_t>(value));
        }
        const auto period = std::chrono::milliseconds(1000 / rate);
        uint64_t bf_sequence = 0;
        uint64_t scan_sequence = 0;
        auto next_scan = std::chrono::steady_clock::now();
        while (generator_running.load())
        {
            if (publisher->HasSession() && service->IsRunning())
            {
                vuprs::net::BfDataHeader header;
                header.sample_rate_millihz = static_cast<uint32_t>(service->SampleRate() * 1000.0);
                header.sample_count = static_cast<uint32_t>(sample_count);
                header.sample_format = static_cast<uint32_t>(vuprs::net::SampleFormat::INT32_Q16_16);
                header.inference_identity = 0;
                header.inference_valid = 0;
                publisher->Publish(vuprs::net::MakeBfFrame(header, samples, ++bf_sequence, vuprs::net::NowNs()));

                const auto now = std::chrono::steady_clock::now();
                if (service->IsScanEnabled() && now >= next_scan)
                {
                    next_scan = now + std::chrono::seconds(1);
                    const size_t data_points = 75;
                    auto values = std::make_shared<std::vector<uint16_t>>(data_points);
                    for (size_t i = 0; i < data_points; i++)
                        (*values)[i] = static_cast<uint16_t>((i * 257 + scan_sequence * 31) % 65536);

                    vuprs::net::ScanDataHeader scan;
                    scan.points_half = static_cast<uint32_t>(service->ScanPoints());
                    scan.data_points = static_cast<uint32_t>(data_points);
                    scan.sample_rate_millihz = static_cast<uint32_t>(service->SampleRate() * 1000.0);
                    scan.alt_min_mdeg = static_cast<int32_t>(service->ScanAltMin() * 1000.0);
                    scan.min_power_mdb = -30000;
                    scan.max_power_mdb = 12000;
                    scan.scan_format = static_cast<uint32_t>(vuprs::net::ScanFormat::UINT16_MINMAX_NORMALIZED);
                    publisher->Publish(vuprs::net::MakeScanFrame(scan, values, ++scan_sequence, vuprs::net::NowNs()));
                }
            }
            std::this_thread::sleep_for(period);
        }
    });

    std::thread io_thread([&]()
                          { server.Io().run(); });

    while (g_running.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    generator_running = false;
    if (generator.joinable())
        generator.join();
    asio::post(server.Io(), [&]()
               { server.Stop(); });
    if (io_thread.joinable())
        io_thread.join();
    std::cout << "[mock] stopped." << std::endl;
    return 0;
}
