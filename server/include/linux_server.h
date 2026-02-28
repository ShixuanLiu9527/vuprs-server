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

            bool configdone;

            std::vector<std::thread> threads;  /* Server threads */

            /* Server */

            int server_fd;
            uint16_t server_port;
            std::atomic<bool> server_running;
            vuprs::ServerConfig server_config;

            std::unique_ptr<vuprs::LinuxSession> server_session;

            /* Algorithms */

            vuprs::ARM_FPGA_BF_Config beamFormerConfig;  /* Set by client or default value */
            vuprs::ARM_FPGA_CollaborationBeamfomer beamformer;  /* System beam former */

            /* Tool functions */

            bool LoadServerConfigFromJson(const std::string& jsonFilename);

            bool InitServer();

            void SessionCallback(int client_fd, const struct sockaddr_in& client_addr, const std::string& message);
            void ConnectCallback(bool connect, const std::string &message);

            /* Thread data */

            std::mutex mut_data;
            std::queue<std::vector<double>> resultQueue;  /* Result queue, controlled by mut_data */

            std::mutex mut_control;
            std::atomic<bool> newTargetDirectionIRQ;
            std::condition_variable beamFormerParameterCV;  /* Beam former parameter CV (when alt, az, ... is change) */

            /* Threads */

            void THREAD__AcceptClient();

            void THREAD__GetResult();

            void THREAD__SendToMaster();

            void THREAD__BeamFormerControl();

            void THREAD__AcceptClient();

        public:

            LinuxServer();
            ~LinuxServer();

            /**
             * @brief Initialize system config JSON files.
             */
            void InitSystemConfigFiles(const vuprs::SystemConfigFiles &config);

            void run();
    };
}

#endif
