/**
 * @brief   This document is the interface of Linux Server of VUPRS.
 * @note    This is the top-level interface of VUPRS system.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef VUPRS_LINUX_SERVER_H
#define VUPRS_LINUX_SERVER_H

#include "server/linux_session.h"
#include "server/protocol.h"
#include "system_tools/string_parse.h"
#include "collab_bf/collaboration_beamformer.h"

#define DEFAULT_SENDING_DATA_QUEUE_LENGTH 10U

#define IS_BINARY_DATA_SENDING_CMD(VAL)                                                \
    ((VAL) == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA) || \
     (VAL) == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER))

namespace vuprs
{
    struct SystemConfigFiles
    {
        std::string fpgaConfigJsonFile;             /* FPGA config JSON file */
        std::string beamFormingArrayConfigJsonFile; /* Beam forming config JSON file */
        std::string firFilterBankConfigJsonFile;    /* FIR filter bank config JSON file */
        std::string serverConfigJsonFile;           /* Server config JSON file */
    };

    struct ServerProtocol
    {
        std::string commandHeader;
        std::string commandTailer;
    };

    struct ServerConfig
    {
        uint16_t initializePort;
        uint16_t maximumPort;
        uint16_t acceptClientCounts;

        vuprs::ServerProtocol protocol;
    };

    /**
     * @brief Linux server.
     *
     * @note Use make_unique
     */
    class LinuxServer
    {
    private:
        bool configdone; /* Indicate config done */

        std::vector<std::thread> threads; /* Server threads */

        /* --- Server --- */

        int server_fd;
        uint16_t server_port;

        std::unique_ptr<vuprs::LinuxSession> server_session; /* socket session */

        std::mutex mut_client_io_manager;                          /* mutex for client io manager */
        std::shared_ptr<vuprs::SocketIOManager> client_io_manager; /* socket io manager for client, controlled by this->mut_client_io_manager */

        /* --- Algorithms --- */

        std::mutex mut_bf;                         /* mutex for beam former */
        vuprs::CollaborationBeamformer beamformer; /* System beam former, controlled by mut_bf */

        std::mutex mut_bf_config;
        vuprs::CollaborationBeamformerConfig beamFormerConfig; /* Set by client or default value, controlled by mut_bf_config */

        std::mutex mut_scan_config;
        vuprs::ScanningConfig scanningConfig; /* Set by client or default value, controlled by mut_scan_config */

        /* --- Thread data --- */

        std::atomic<bool> server_running; /* server running */

        std::mutex mut_server_config;
        vuprs::ServerConfig server_config; /* controlled by mut_server_config */

        std::mutex mut_response;
        std::atomic<bool> serverResponseIRQ;
        std::atomic<bool> serverNeedResponse; /* false = no need to send response */
        std::condition_variable serverResponseCV;
        std::string serverResponseMessage;

        std::mutex mut_send;
        std::condition_variable sendingCV;
        std::atomic<bool> sendingIRQ;        /* true: should send */
        std::atomic<uint32_t> sendingFormat; /* Sending format: data, current alg param */

        std::mutex mut_control;
        std::atomic<bool> controlIRQ;
        std::condition_variable controlCV; /* Beam former parameter CV (when alt, az, ... is change) */

        std::mutex mut_cmd;
        vuprs::ServerCommandInformation cmdINFO; /* Command information, parsed from message, controlled by mut_cmd */

        /* --- Threads --- */

        /**
         * @brief Accept client thread, which will block in accept() and wait for client connection.
         */
        void THREAD__AcceptClient();

        /**
         * @brief Send thread, which will block in sendingCV and wait for sendingIRQ, then send data to client.
         *
         * @note Fork 1: send result data (when this->sendingFormat is SERVER_CMD__GET_NEW_DATA, send data in this->resultQueue).
         * @note Fork 2: send current algorithm parameters (when this->sendingFormat is SERVER_CMD__GET_ALG_PARAM, send current algorithm parameters).
         */
        void THREAD__Send();

        /**
         * @brief Control thread, handle command from client.
         *
         * @note When receive command, control thread will handle the command and
         * @note set serverResponseIRQ to true, then notify serverResponseCV to send response to client.
         */
        void THREAD__Control();

        /**
         * @brief Client session callback.
         *
         * @note This function is called when receive message from client,
         * @note and the message is parsed to command information.
         *
         * @note Then the command information is stored in this->cmdINFO,
         * @note and this->controlIRQ is set to true to notify control thread to handle the command.
         *
         * @note After handling the command, control thread will set this->serverResponseIRQ to true
         * @note and notify this->serverResponseCV to send response of this operation to client.
         */
        void SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string &message);

        /* --- Tool functions --- */

        bool LoadServerConfigFromJson(const std::string &jsonFilename);

        bool InitServer();

        void ConnectCallback(bool connect, const std::string &message);

    public:
        LinuxServer();
        ~LinuxServer();

        /**
         * @brief Initialize system config JSON files.
         */
        void InitSystemConfigFiles(const vuprs::SystemConfigFiles &config);

        /**
         * @brief Start server.
         */
        void Run();

        /**
         * @brief Stop server.
         */
        void Stop();

        bool ConfigDone() const;
    };
}

#endif
