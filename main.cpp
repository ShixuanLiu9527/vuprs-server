#include <signal.h>
#include "linux_server.h"
#include "logger/log_manager.h"

static void ParseConfigFIlesFromARGV(const std::vector<std::string> &args, vuprs::SystemConfigFiles *configs)
{
    PARAM_CHECK(args.size() == 9, "vuprs-server", " Invalid command with arg size = " + std::to_string(args.size()));
    PARAM_CHECK(args[1] == "--server-config" && args[3] == "--fpga-config" && args[5] == "--array-config" && args[7] == "--fir-config", "vuprs-server", " Invalid command.");

    configs->serverConfigJsonFile = args[2];
    configs->fpgaConfigJsonFile = args[4];
    configs->beamFormingArrayConfigJsonFile = args[6];
    configs->firFilterBankConfigJsonFile = args[8];
}

/**
 * @note Command (argc = 9): server --server-config {JSON} --fpga-config {JSON} --array-config {JSON} --fir-config {JSON}
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
    ParseConfigFIlesFromARGV(args, &configs);

    /* Start server */

    vuprs::LinuxServer server;

    server.InitSystemConfigFiles(configs);

    RUNTIME_CHECK(server.ConfigDone(), "vuprs-server", " Error occurred in configuration, the server has been killed.");

    server.Run();  /* Start */

    int val;
    std::cin >> val;

    server.Stop();

    return 0;
}
