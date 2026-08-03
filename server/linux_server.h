#ifndef VUPRS_LINUX_SERVER_H
#define VUPRS_LINUX_SERVER_H

#include "server/linux_session.h"
#include "server/protocol.h"
#include "system_tools/string_parse.h"
#include "logger/log_manager.h"
#include "hybrid/hybrid_bf.h"
#include "fault_detect/fault_detector.h"

#define DEFAULT_SENDING_DATA_QUEUE_LENGTH 10U

#define IS_BINARY_DATA_SENDING_CMD(VAL)                                                \
    ((VAL) == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA) || \
     (VAL) == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER))

namespace vuprs
{
    struct SystemLoggerConfig
    {
        std::string hybrid_logger_dir;
        std::string server_logger_dir;
        std::string inference_logger_dir;
        SystemLoggerConfig() = default;
        bool InitFromJson(const std::string &filename);
    };

    struct SystemConfigFiles
    {
        std::string server_config_json;          /* arg[2] - Server config JSON file */
        std::string fpga_config_json;            /* arg[4] - FPGA config JSON file */
        std::string bf_array_config_json;        /* arg[6] - Beam forming config JSON file */
        std::string fir_config_json;             /* arg[8] - FIR filter bank config JSON file */
        std::string inference_model_config_json; /* arg[10] - Inference model config json */
        SystemLoggerConfig logger_configs;       /* arg[12] - Logger configs */
    };

    struct ServerProtocol
    {
        std::string command_header;
        std::string command_tailer;
    };

    struct ServerConfig
    {
        uint16_t initialize_port;
        uint16_t maximum_port;
        uint16_t accept_client_counts;
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
        bool config_done;                 /* Indicate config done */
        std::vector<std::thread> threads; /* Server threads */
        std::shared_ptr<spdlog::logger> server_logger;

        /* --- Server --- */

        int server_fd;
        uint16_t server_port;

        std::unique_ptr<LinuxSession> server_session; /* socket session */

        std::mutex mut_client_io_manager;                   /* mutex for client io manager */
        std::shared_ptr<SocketIOManager> client_io_manager; /* socket io manager for client, controlled by this->mut_client_io_manager */

        /* --- Algorithms --- */

        std::mutex mut_bf;           /* mutex for beam former */
        HybridBeamformer beamformer; /* System beam former, controlled by mut_bf */

        std::mutex mut_bf_config;
        HybridBeamformerConfig bf_config; /* Set by client or default value, controlled by mut_bf_config */

        std::mutex mut_scan_config;
        ScanningConfig scan_config; /* Set by client or default value, controlled by mut_scan_config */

        std::mutex mut_npu;
        FaultDetector fault_detector;

        /* --- Thread data --- */

        std::atomic<bool> server_running; /* server running */

        std::mutex mut_server_config;
        vuprs::ServerConfig server_config; /* controlled by mut_server_config */

        std::mutex mut_response;
        std::atomic<bool> server_response_irq;
        std::atomic<bool> server_need_response; /* false = no need to send response */
        std::condition_variable server_response_cv;
        std::string server_response_message;

        std::mutex mut_send;
        std::condition_variable sending_cv;
        std::atomic<bool> sending_irq;        /* true: should send */
        std::atomic<uint32_t> sending_format; /* Sending format: data, current alg param */

        std::mutex mut_control;
        std::atomic<bool> control_irq;
        std::condition_variable control_cv; /* Beam former parameter CV (when alt, az, ... is change) */

        std::mutex mut_cmd;
        vuprs::ServerCommandInformation cmd_info; /* Command information, parsed from message, controlled by mut_cmd */

        /* --- Threads --- */

        /**
         * @brief Accept client thread, which will block in accept() and wait for client connection.
         */
        void THREAD__AcceptClient();

        /**
         * @brief Send thread, which will block in sending_cv and wait for sending_irq, then send data to client.
         *
         * @note Fork 1: send result data (when this->sending_format is SERVER_CMD__GET_NEW_DATA, send data in this->result_queue).
         * @note Fork 2: send current algorithm parameters (when this->sending_format is SERVER_CMD__GET_ALG_PARAM, send current algorithm parameters).
         */
        void THREAD__Send();

        /**
         * @brief Control thread, handle command from client.
         *
         * @note When receive command, control thread will handle the command and
         * @note set server_response_irq to true, then notify server_response_cv to send response to client.
         */
        void THREAD__Control();

        /**
         * @brief Client session callback.
         *
         * @note This function is called when receive message from client,
         * @note and the message is parsed to command information.
         *
         * @note Then the command information is stored in this->cmd_info,
         * @note and this->control_irq is set to true to notify control thread to handle the command.
         *
         * @note After handling the command, control thread will set this->server_response_irq to true
         * @note and notify this->server_response_cv to send response of this operation to client.
         */
        void SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string &message);

        /* --- Tool functions --- */

        bool LoadServerConfigFromJson(const std::string &json_filename);

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
