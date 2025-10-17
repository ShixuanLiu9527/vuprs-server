#include "session_callback.h"

vuprs::FPGAController fpgaController;  /* FPGA controller */
std::shared_ptr<spdlog::logger> algorithmLogger;

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- Algorithm logger ------------------------------------------------ */
/* --------------------------------------------------------------------------------------------------------------- */

void algorithm_Info(const std::string &info);
void algorithm_Warn(const std::string &warn);
void algorithm_Error(const std::string &err);
void algorithm_Critical(const std::string &critical);

void algorithm_Info(const std::string &info) {if (algorithmLogger) algorithmLogger->info(info);}
void algorithm_Warn(const std::string &warn) {if (algorithmLogger) algorithmLogger->warn(warn);}
void algorithm_Error(const std::string &err) {if (algorithmLogger) algorithmLogger->error(err);}
void algorithm_Critical(const std::string &critical) {if (algorithmLogger) algorithmLogger->critical(critical);}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- Connect callback ------------------------------------------------ */
/* --------------------------------------------------------------------------------------------------------------- */

void vuprs::SystemConnectCallback(bool connected, const std::string& clientInfo)
{
    
}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- Session callback ------------------------------------------------ */
/* --------------------------------------------------------------------------------------------------------------- */

void vuprs::SystemSessionCallback(int client_fd, const struct sockaddr_in& client_addr, const std::string& message)
{
    
}
