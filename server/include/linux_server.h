/**
 * @brief   This document is the interface of Linux Server of VUPRS.
 * @note    This is the top-level interface of VUPRS system.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef VUPRS_LINUX_SERVER_H
#define VUPRS_LINUX_SERVER_H

#include "tcp_server.h"
#include "fpga_config.h"
#include "log_manager.h"
#include "session_callback.h"

namespace vuprs
{
    struct SystemLoggerConfig
    {
        std::string linux_server_logger_name;
        std::string linux_server_logger_filename;

        std::string fpga_controller_logger_name;
        std::string fpga_controller_logger_filename;

        std::string algorithm_logger_name;
        std::string algorithm_logger_filename;
    };

    struct SystemConfigFiles
    {
        std::string fpga_config_json_file;
        std::string server_config_json_file;
    };

    class LinuxServer
    {
        private:

            vuprs::TcpServer server;
            vuprs::FPGAConfigManager fpgaConfigManager;  /* FPGA configuration */
            bool fileConfigDown;

            bool SystemSelfTest();

            bool StartUpCondition();

        public:

            LinuxServer();
            ~LinuxServer();

            /**
             * @brief Initialize system logger.
             */
            void InitSystemLogger(const vuprs::SystemLoggerConfig &systemLoggerConfig);

            /**
             * @brief Initialize system config JSON files.
             */
            void InitSystemConfigFiles(const vuprs::SystemConfigFiles &systemConfigFiles);

            void run();
    };
}

#endif
