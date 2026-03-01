#include <signal.h>
#include "linux_server.h"

static void ParseConfigFIlesFromARGV(const std::vector<std::string> &args, vuprs::SystemConfigFiles *configs)
{
    if (args.size() != 9)
    {
        throw std::runtime_error("Invalid command with arg size = " + std::to_string(args.size()));
    }
    if (args[1] != "--server-config" || args[3] != "--fpga-config" || args[5] != "--array-config" || args[7] != "--fir-config")
    {
        throw std::runtime_error("Invalid command.");
    }

    configs->serverConfigJsonFile = args[2];
    configs->fpgaConfigJsonFile = args[4];
    configs->beamFormingArrayConfigJsonFile = args[6];
    configs->firFilterBankConfigJsonFile = args[8];
}

static std::condition_variable system_cv;
static std::mutex system_mut;
static std::atomic<bool> g_stop = false;

static void SystemSignalHandler(int) 
{
    g_stop = true;
    system_cv.notify_one();
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

    /* Set signal */

    signal(SIGINT, SystemSignalHandler);  /* enable Ctrl+C interrupt */
    signal(SIGTERM, SystemSignalHandler);  /* enable kill */

    /* Start server */

    vuprs::LinuxServer server;

    server.InitSystemConfigFiles(configs);

    if (!server.ConfigDone())
    {
        throw std::runtime_error("Error occurred in configuration, the server has been killed.");
    }

    server.Run();  /* Start */

    {
        std::unique_lock<std::mutex> lock(system_mut);  /* LOCK */
        system_cv.wait(lock, []{return g_stop.load();});
    }

    server.Stop();

    return 0;
}
