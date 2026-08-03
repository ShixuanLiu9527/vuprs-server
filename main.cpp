#include <signal.h>
#include "server/linux_server.h"
#include "logger/check.h"

static void ParseArgs(const std::vector<std::string> &args, vuprs::SystemConfigFiles *configs)
{
    PARAM_CHECK(args.size() == 13, "vuprs-server", " Invalid command with arg size = " + std::to_string(args.size()));
    PARAM_CHECK(args[1] == "--server-config" &&
                    args[3] == "--fpga-config" &&
                    args[5] == "--array-config" &&
                    args[7] == "--fir-config" &&
                    args[9] == "--inference-config" &&
                    args[11] == "--logger-config",
                "vuprs-server", " Invalid command.");
    configs->server_config_json = args[2];
    configs->fpga_config_json = args[4];
    configs->bf_array_config_json = args[6];
    configs->fir_config_json = args[8];
    configs->inference_model_config_json = args[10];
    configs->logger_configs.InitFromJson(args[12]);
}

/**
 * @note Command (argc = 13): server --server-config {JSON}
 *                                   --fpga-config {JSON}
 *                                   --array-config {JSON}
 *                                   --fir-config {JSON}
 *                                   --inference-config {JSON}
 *                                   --logger-config {JSON}
 */
int main(int argc, char *argv[])
{
    std::vector<std::string> args;
    args.resize(argc);
    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
    }
    /* Parse args */
    vuprs::SystemConfigFiles configs;
    ParseArgs(args, &configs);
    /* Start server */
    vuprs::LinuxServer server;
    server.InitSystemConfigFiles(configs);
    RUNTIME_CHECK(server.ConfigDone(), "vuprs-server", " Error occurred in configuration, the server has been killed.");
    server.Run(); /* Start */
    int val;
    std::cin >> val;
    server.Stop();
    return 0;
}
