#include "linux_server.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    this->fileConfigDown = false;
}

vuprs::LinuxServer::~LinuxServer()
{

}

bool vuprs::LinuxServer::SystemSelfTest()
{
    /* Test FPGA */
}

bool vuprs::LinuxServer::StartUpCondition()
{
    return this->fileConfigDown;
}

void vuprs::LinuxServer::InitSystemLogger(const vuprs::SystemLoggerConfig &systemLoggerConfig)
{
    /* Initialize server */

    this->server.InitLogger(
        systemLoggerConfig.linux_server_logger_name,
        systemLoggerConfig.linux_server_logger_filename
    );

    /* Initialize FPGA Controller logger */

    fpgaController.InitLogger(
        systemLoggerConfig.fpga_controller_logger_name,
        systemLoggerConfig.fpga_controller_logger_filename
    );

    /* Initialize algorithm logger */

    algorithmLogger = vuprs::LogManager::getLogger(
        systemLoggerConfig.algorithm_logger_name,
        systemLoggerConfig.algorithm_logger_filename
    );
}

void vuprs::LinuxServer::InitSystemConfigFiles(const vuprs::SystemConfigFiles &systemConfigFiles)
{
    this->server.LoadConfigFromJson(systemConfigFiles.server_config_json_file);
    if(!this->fpgaConfigManager.LoadFPGAConfigFromJson(systemConfigFiles.fpga_config_json_file))
    {
        throw std::runtime_error("FPGA controller load configuration file error.");
    }
    fpgaController.LoadFPGAConfig(this->fpgaConfigManager);
    this->fileConfigDown = true;
}

void vuprs::LinuxServer::run()
{
    if (this->StartUpCondition())
    {
        this->server.SetServerConnectionCallback(vuprs::SystemConnectCallback);
        this->server.SetSessionMessageHandler(vuprs::SystemSessionCallback);

        this->server.start();

        while (true) {std::this_thread::sleep_for(std::chrono::milliseconds(100));}
    }
    else
    {
        throw std::runtime_error("Initialize not complete.");
    }
}
