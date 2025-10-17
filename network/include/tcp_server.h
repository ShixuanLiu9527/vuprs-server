#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <memory>
#include <thread>
#include <atomic>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>

#include "tcp_session.h"
#include "nerwork_exception.h"
#include "string_parse.h"
#include "nlohmann/json.hpp"
#include "log_manager.h"

#define __DEFAULT_INITIALIZE_PORT__        8080
#define __DEFAULT_MAX_PORT__               10000
#define __DEFAULT_ACCEPT_CLIENT_COUNT__    1

namespace vuprs
{
    /**
     * @brief use: void func(bool connected, const std::string& clientInfo);
     */
    using ServerConnectionCallback = std::function<void(bool connected, const std::string& clientInfo)>;

    struct ServerConfig
    {
        uint16_t initializePort;
        uint16_t maximumPort;
        uint16_t acceptClientCounts;
    };
    
    class TcpServer {

        private:
        
            uint16_t port;
            int server_fd;
            ServerConfig serverConfig;
            std::shared_ptr<spdlog::logger> serverLogger;

            std::atomic<bool> running;  /* running status */
            std::thread serverThread;  /* Listening thread */
            std::unique_ptr<vuprs::TcpSession> currentSession;

            ServerConnectionCallback serverConnectionCallback;
            vuprs::SessionMessageHandler sessionMessageHandler;  /* give to session */

            bool initializeSocket();
            void acceptConnection();

            /* log */

            void Info(const std::string &info);
            void Warn(const std::string &warn);
            void Error(const std::string &err);
            void Critical(const std::string &critical);

        public:

            TcpServer(const std::string& configJsonFilename);
            TcpServer(const uint64_t& port = 8080);
            ~TcpServer();

            TcpServer(const TcpServer&) = delete;
            TcpServer& operator=(const TcpServer&) = delete;

            void start();
            void stop();
            bool isRunning() const;

            bool LoadConfigFromJson(const std::string& jsonFilename);
            void InitLogger(const std::string &loggerName, const std::string &loggerFilename);
            bool SendToClient(const std::string& message);

            /**
             * @brief Define connect callback operate function.
             * @note 1. The input function will be called when connection is established;
             *       2. Show some message when connected and disconnected.
             * @param callback call back function pointer.
             */
            void SetServerConnectionCallback(ServerConnectionCallback callback);

            /**
             * @brief Define message handler operate function (for session).
             * @note 1. The input function will be given to session when connected;
             *       2. Perform the main operation in this function (FPGA control & Fault processing).
             * @param handler call back function pointer.
             */
            void SetSessionMessageHandler(vuprs::SessionMessageHandler handler);
    };
}

#endif
