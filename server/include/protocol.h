#ifndef LINUX_SERVER_PROTOCOL_H
#define LINUX_SERVER_PROTOCOL_H

#include <string>
#include <arpa/inet.h>

#include "arm_fpga_bf_collab.h"

namespace vuprs
{
    /**
     * @brief Server command format.
     */
    enum class ServerCommand
    {
        SERVER_CMD__INVALID = 0,  /* Invalid command */
        SERVER_CMD__RESET,  /* Reset beam former (STEP 1: Stop, STEP 2: Clear) */
        SERVER_CMD__REDIRECT,  /* Redirect beam former */
        SERVER_CMD__CHANGE_BEAMFORMER,  /* Change beam former */
        SERVER_CMD__CHANGE_ALG_PARAM,  /* Change algorithm parameters (STEP 1: Stop, STEP 2: Start with new parameters) */
        SERVER_CMD__STOP,  /* Stop beam former */
        SERVER_CMD__START,  /* Start beam former */
        SERVER_CMD__GET_NEW_DATA  /* Get newest data from server (Send DMA buffer to host) */
    };

    struct ServerCommandInformation
    {
        vuprs::ServerCommand cmd;
        ARM_FPGA_BF_Config config;  /* config info */
        std::string beamformer_name;  /* beam former name (for SERVER_CMD__CHANGE_BEAMFORMER) */
    };

    bool PROTOCOL_ParseCommandFromMessage(const std::string &message, ServerCommandInformation *cmd);
    
    std::string PROTOCOL_MakeServerResponse(const ServerCommandInformation &cmd, bool operationStatus);
}

#endif
