#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "asio.hpp"
#include "server/net/asio_server.h"
#include "server/server_core.h"
#include "logger/check.h"

static void PrintUsage(const char *program)
{
    std::cout << "Usage: " << program << "\n"
              << "  --server-config <json>\n"
              << "  --fpga-config <json>\n"
              << "  --array-config <json>\n"
              << "  --fir-config <json>\n"
              << "  --inference-config <json>\n"
              << "  --hybrid-bf-default-config <json>\n"
              << "  --logger-config <json>\n"
              << "  --help\n";
}

static void ParseArgs(const std::vector<std::string> &args, vuprs::SystemConfigFiles *configs)
{
    PARAM_CHECK(args.size() == 15, "vuprs-server", " Invalid command with arg size = " + std::to_string(args.size()));
    PARAM_CHECK(args[1] == "--server-config" &&
                    args[3] == "--fpga-config" &&
                    args[5] == "--array-config" &&
                    args[7] == "--fir-config" &&
                    args[9] == "--inference-config" &&
                    args[11] == "--hybrid-bf-default-config" &&
                    args[13] == "--logger-config",
                "vuprs-server", " Invalid command.");
    configs->server_config_json = args[2];
    configs->fpga_config_json = args[4];
    configs->bf_array_config_json = args[6];
    configs->fir_config_json = args[8];
    configs->inference_model_config_json = args[10];
    configs->hybrid_default_config_json = args[12];
    configs->logger_configs.InitFromJson(args[14]);
}

int main(int argc, char *argv[])
{
    std::vector<std::string> args;
    args.resize(argc);
    for (int i = 0; i < argc; i++)
        args[i] = std::string(argv[i]);

    if (argc == 2 && (args[1] == "--help" || args[1] == "-h"))
    {
        PrintUsage(argv[0]);
        return 0;
    }

    try
    {
#ifdef SIGPIPE
        std::signal(SIGPIPE, SIG_IGN);
#endif

        vuprs::SystemConfigFiles configs;
        ParseArgs(args, &configs);

        auto core = std::make_shared<vuprs::ServerCore>();
        core->InitSystemConfigFiles(configs);
        RUNTIME_CHECK(core->ConfigDone(), "vuprs-server", "Error occurred in configuration, the server has been killed.");

        vuprs::net::AsioServerConfig net_config;
        net_config.bind_address = "0.0.0.0";
        net_config.port = core->Port();
        net_config.max_payload_bytes = core->MaxPayloadBytes();
        net_config.data_queue_frames = core->DataQueueFrames();
        net_config.control_queue_frames = core->ControlQueueFrames();
        vuprs::net::AsioServer server(net_config, core);
        core->SetPublisher(server.Publisher());

        RUNTIME_CHECK(core->Run(), "vuprs-server", "Failed to start beamformer.");
        std::string error;
        if (!server.Start(&error))
        {
            core->Stop();
            RUNTIME_CHECK(false, "vuprs-server", " network start failed: " + error);
        }

        asio::signal_set signals(server.Io(), SIGINT, SIGTERM);
        signals.async_wait([&server](const asio::error_code &, int)
                           { server.Stop(); });

        std::cout << "[server] listening on port " << server.BoundPort()
                  << " (protocol 1, control JSON + binary data push)" << std::endl;
        server.Io().run();
        core->Stop();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[vuprs-server] " << e.what() << std::endl;
        return 1;
    }
}
