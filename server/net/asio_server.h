#ifndef VUPRS_NET_ASIO_SERVER_H
#define VUPRS_NET_ASIO_SERVER_H

#include <cstdint>
#include <memory>
#include <string>
#include "asio.hpp"
#include "server/net/control_worker.h"
#include "server/net/frame_publisher.h"
#include "server/net/session.h"

namespace spdlog
{
    class logger;
}

namespace vuprs
{
    namespace net
    {
        struct AsioServerConfig
        {
            std::string bind_address = "0.0.0.0";
            uint16_t port = 8080;
            size_t max_payload_bytes = MAX_PAYLOAD_BYTES;
            size_t data_queue_frames = 64;
            size_t control_queue_frames = 16;
            size_t control_worker_queue = 64;
            std::shared_ptr<spdlog::logger> logger; /* optional; defaults to spdlog::default_logger() */
        };

        /**
         * @brief Single-port Asio server for the VUPRS protocol.
         *
         * Lifecycle: Start() before running Io(); Stop() from the io thread
         * (e.g. via asio::post) or after Io().run() has returned.
         */
        class AsioServer
        {
        public:
            AsioServer(AsioServerConfig config, std::shared_ptr<IControlService> service);
            ~AsioServer();

            AsioServer(const AsioServer &) = delete;
            AsioServer &operator=(const AsioServer &) = delete;

            bool Start(std::string *error = nullptr);
            void Stop();

            asio::io_context &Io() { return this->io_; }
            std::shared_ptr<FramePublisher> Publisher() const { return this->publisher_; }
            uint16_t BoundPort() const { return this->bound_port_; }

        private:
            void DoAccept();
            void OnAccept(const asio::error_code &ec, asio::ip::tcp::socket socket);
            void OnSessionClosed(Session *session);

            AsioServerConfig config_;
            std::shared_ptr<IControlService> service_;
            asio::io_context io_;
            asio::ip::tcp::acceptor acceptor_;
            std::shared_ptr<ControlWorker> control_;
            std::shared_ptr<FramePublisher> publisher_;
            std::shared_ptr<Session> session_;
            uint16_t bound_port_ = 0;
            bool started_ = false;
            bool stopped_ = false;
        };
    }
}

#endif
