#ifndef LINUX_SERVER_PROTOCOL_H
#define LINUX_SERVER_PROTOCOL_H

#include <string>
#ifdef __linux__
#include <arpa/inet.h>
#endif
#include "hybrid/hybrid_bf_config.h"
#include "3rdparty/nlohmann/json.hpp"

#define SERVER_CMD__INVALID__STR "invalid"
#define SERVER_CMD__ACK__STR "ack"
#define SERVER_CMD__RESET__STR "reset"
#define SERVER_CMD__REDIRECT__STR "redirect"
#define SERVER_CMD__CHANGE_BEAMFORMER__STR "change_beamformer"
#define SERVER_CMD__CHANGE_ALG_PARAM__STR "change_algorithm_parameters"
#define SERVER_CMD__STOP__STR "stop"
#define SERVER_CMD__START__STR "start"
#define SERVER_CMD__GET_NEW_DATA__STR "get_data"
#define SERVER_CMD__SCAN_FOR_POSITION_POWER__STR "power_scan"
#define SERVER_CMD__GET_ALG_PARAM__STR "read_algorithm_parameters"

namespace vuprs
{
    /**
     * @brief Server command format.
     */
    enum class ServerCommand
    {
        SERVER_CMD__INVALID = 0,             /* Invalid command */
        SERVER_CMD__ACK,                     /* Acknowledge command, which is used to indicate that the server has received the command and is processing it, and the client can wait for response. */
        SERVER_CMD__RESET,                   /* Reset beam former (STEP 1: Stop, STEP 2: Clear) */
        SERVER_CMD__REDIRECT,                /* Redirect beam former */
        SERVER_CMD__CHANGE_BEAMFORMER,       /* Change beam former */
        SERVER_CMD__CHANGE_ALG_PARAM,        /* Change algorithm parameters (STEP 1: Stop, STEP 2: Start with new parameters) */
        SERVER_CMD__STOP,                    /* Stop beam former */
        SERVER_CMD__START,                   /* Start beam former */
        SERVER_CMD__GET_NEW_DATA,            /* Get newest data from server (Send DMA buffer to host) */
        SERVER_CMD__SCAN_FOR_POSITION_POWER, /* Scan for position power */
        SERVER_CMD__GET_ALG_PARAM            /* Get current algorithm parameters */
    };

    /**
     * @note Default value of cmd is SERVER_CMD__INVALID, which means invalid command.
     */
    struct ServerCommandInformation
    {
        vuprs::HybridBeamformerConfig config; /* config info */
        vuprs::ScanningConfig scan_config;    /* scanning config info, which is used when cmd is SERVER_CMD__SCAN_FOR_POSITION_POWER */
        vuprs::ServerCommand cmd;
        std::string beamformer_name; /* beam former name (for SERVER_CMD__CHANGE_BEAMFORMER) */
        ServerCommandInformation() : cmd(vuprs::ServerCommand::SERVER_CMD__INVALID), beamformer_name("") {}
    };

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
     * @param info Additional information for the response.
     * @param operation-status Operation status of command (true: success, false: failed).
     *
     * @retval Server response message.
     */
    std::string PROTOCOL_MakeServerOperationResponse(const ServerCommandInformation &cmd,
                                                     const std::string &info = "",
                                                     bool operation_status = true);

    /**
     * @brief Make server response message for current algorithm parameters.
     *
     * @param config Current algorithm parameters.
     */
    std::string PROTOCOL_MakeServerParameterResponse(const vuprs::HybridBeamformerConfig &config);

    std::string PROTOCOL_MakeServerScanningResponse(const vuprs::ScanningConfig &scan_config,
                                                    double min_scan_power_dB,
                                                    double max_scan_power_dB,
                                                    const std::string &info = "",
                                                    bool operation_status = true);

    std::string PROTOCOL_MakeServerResultDataResponse(const std::string &info = "", bool operation_status = true);
}

#endif
