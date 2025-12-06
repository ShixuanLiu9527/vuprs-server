#ifndef SESSION_CALLBACK_H
#define SESSION_CALLBACK_H

#include "tcp_session.h"
#include "fpga_control.h"

extern vuprs::FPGAController fpgaController;
extern std::shared_ptr<spdlog::logger> algorithmLogger;

namespace vuprs
{
    void SystemSessionCallback(int client_fd, const struct sockaddr_in& client_addr, const std::string& message);
    void SystemConnectCallback(bool connected, const std::string& clientInfo);
}

#endif
