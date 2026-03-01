/**
 * @brief   This document is the interface of Linux Server of VUPRS.
 * @note    This is the top-level interface of VUPRS system.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef VUPRS_LINUX_SERVER_H
#define VUPRS_LINUX_SERVER_H

#include "linux_session.h"
#include "string_parse.h"
#include "arm_fpga_bf_collab.h"
#include "protocol.h"

#define DEFAULT_SENDING_DATA_QUEUE_LENGTH 10U

namespace vuprs
{
    struct SystemConfigFiles
    {
        std::string fpgaConfigJsonFile;  /* FPGA config JSON file */
        std::string beamFormingArrayConfigJsonFile;  /* Beam forming config JSON file */
        std::string firFilterBankConfigJsonFile;  /* FIR filter bank config JSON file */
        std::string serverConfigJsonFile;  /* Server config JSON file */
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

            bool configdone;  /* Indicate config done */

            std::vector<std::thread> threads;  /* Server threads */

            /* --- Server --- */

            int server_fd;
            uint16_t server_port;

            std::unique_ptr<vuprs::LinuxSession> server_session;  /* socket session */
            std::shared_ptr<vuprs::SocketIOManager> socketIOManager;  /* socket io manager */

            /* --- Algorithms --- */

            vuprs::ARM_FPGA_BF_Config beamFormerConfig;  /* Set by client or default value */
            vuprs::ARM_FPGA_CollaborationBeamfomer beamformer;  /* System beam former */

            /* --- Thread data --- */

            std::atomic<bool> server_running;  /* server running */

            std::mutex mut_config;
            vuprs::ServerConfig server_config;  /* controlled by mut_config */

            std::mutex mut_readResult;
            std::condition_variable readResultCV;
            std::atomic<bool> readResultIRQ;  /* true: should send */

            std::queue<std::vector<double>> resultQueue;  /* Result queue (read from hardware), controlled by mut_readResult */

            std::mutex mut_response;
            std::atomic<bool> serverResponseIRQ;
            std::atomic<bool> serverNeedResponse;  /* false = no need to send response */
            std::condition_variable serverResponseCV;
            std::string serverResponseMessage;

            std::mutex mut_send;
            std::condition_variable resultSendingCV;
            std::atomic<bool> resultSendingIRQ;  /* true: should send */

            std::mutex mut_control;
            std::atomic<bool> controlIRQ;
            std::condition_variable controlCV;  /* Beam former parameter CV (when alt, az, ... is change) */

            std::mutex mut_cmd;
            vuprs::ServerCommandInformation cmdINFO;  /* Command information, parsed from message, controlled by mut_cmd */

            /* --- Threads --- */

            void THREAD__AcceptClient();

            void THREAD__GetResult();

            void THREAD__SendToMaster();

            void THREAD__Control();

            /**
             * @brief Client session callback.
             */
            void SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message);

            /* --- Tool functions --- */

            bool LoadServerConfigFromJson(const std::string& jsonFilename);

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
