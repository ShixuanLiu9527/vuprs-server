#ifndef LINUX_SERVER_PROTOCOL_H
#define LINUX_SERVER_PROTOCOL_H

#include <string>
#include <arpa/inet.h>

#include "arm_fpga_bf_collab.h"
#include "nlohmann/json.hpp"

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

    /**
     * @note Default value of cmd is SERVER_CMD__INVALID, which means invalid command.
     */
    struct ServerCommandInformation
    {
        vuprs::ServerCommand cmd;
        ARM_FPGA_BF_Config config;  /* config info */
        std::string beamformer_name;  /* beam former name (for SERVER_CMD__CHANGE_BEAMFORMER) */

        ServerCommandInformation() : cmd(vuprs::ServerCommand::SERVER_CMD__INVALID), beamformer_name("") {}
    };

    /**
     * @brief Remove frame header/tailer if exists on boundaries.
     *
     * @note If header exists at message beginning, remove it.
     * @note If tailer exists at message ending, remove it.
     * @note If not found, keep message unchanged.
     */
    std::string RemoveFrameIfExists(const std::string &message, const std::string &header, const std::string &tailer);

    /**
     * @brief Ensure frame header/tailer exist on boundaries.
     *
     * @note If both header and tailer already exist, keep unchanged.
     * @note Otherwise add missing part(s).
     */
    std::string AddFrameIfMissing(const std::string &message, const std::string &header, const std::string &tailer);

    /**
     * @brief Parse command information from message.
     * 
     * @note The given message should not contain header and tailer, 
     * @note which should be removed before calling this function.
     * 
     * @param message Message received from client.
     * @param cmd Output command information.
     * 
     * @retval true: if parse successfully;
     * @retval false: otherwise.
     */
    bool PROTOCOL_ParseCommandFromMessage(const std::string &message, ServerCommandInformation *cmd);
    
    /**
     * @brief Make server response message.
     * 
     * @note The returned message does not contain header and tailer, 
     * @note which should be added before sending to client.
     * 
     * @param cmd Command information.
     * @param operationStatus Operation status of command (true: success, false: failed).
     * 
     * @retval Server response message.
     */
    std::string PROTOCOL_MakeServerResponse(const ServerCommandInformation &cmd, bool operationStatus);
}

#endif
