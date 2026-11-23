#include "server/net/asio_server.h"

#include <iostream>

vuprs::net::AsioServer::AsioServer(AsioServerConfig config, std::shared_ptr<IControlService> service)
    : config_(std::move(config)),
      service_(std::move(service)),
      acceptor_(io_),
      publisher_(std::make_shared<FramePublisher>())
{
    if (this->config_.data_queue_frames == 0)
        this->config_.data_queue_frames = 1;
    if (this->config_.control_queue_frames == 0)
        this->config_.control_queue_frames = 1;
    if (this->config_.control_worker_queue == 0)
        this->config_.control_worker_queue = 1;
    if (this->config_.max_payload_bytes == 0)
        this->config_.max_payload_bytes = MAX_PAYLOAD_BYTES;
}

vuprs::net::AsioServer::~AsioServer()
{
    this->Stop();
}

bool vuprs::net::AsioServer::Start(std::string *error)
{
    if (this->started_)
        return true;
    if (!this->service_)
    {
        if (error)
            *error = "control service is null";
        return false;
    }

    asio::error_code ec;
    const auto address = asio::ip::make_address(this->config_.bind_address, ec);
    if (ec)
    {
        if (error)
            *error = "invalid bind address: " + ec.message();
        return false;
    }
    const asio::ip::tcp::endpoint endpoint(address, this->config_.port);

    this->acceptor_.open(endpoint.protocol(), ec);
    if (!ec)
        this->acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (!ec)
        this->acceptor_.bind(endpoint, ec);
    if (!ec)
        this->acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        const std::string message = "cannot listen on " + this->config_.bind_address + ":" +
                                    std::to_string(this->config_.port) + ": " + ec.message();
        this->acceptor_.close(ec);
        if (error)
            *error = message;
        return false;
    }

    this->bound_port_ = this->acceptor_.local_endpoint(ec).port();
    this->control_ = std::make_shared<ControlWorker>(this->service_, this->config_.control_worker_queue);
    this->control_->Start();
    this->started_ = true;
    this->stopped_ = false;
    this->DoAccept();
    return true;
}

void vuprs::net::AsioServer::Stop()
{
    if (this->stopped_)
        return;
    this->stopped_ = true;
    asio::error_code ec;
    this->acceptor_.close(ec);
    if (this->session_)
    {
        this->publisher_->ResetSession(this->session_.get());
        this->session_->Close();
        this->session_.reset();
    }
    if (this->control_)
        this->control_->Stop();
    this->io_.stop();
}

void vuprs::net::AsioServer::DoAccept()
{
    if (this->stopped_)
        return;
    this->acceptor_.async_accept(
        [this](const asio::error_code &ec, asio::ip::tcp::socket socket)
        {
            this->OnAccept(ec, std::move(socket));
        });
}

void vuprs::net::AsioServer::OnAccept(const asio::error_code &ec, asio::ip::tcp::socket socket)
{
    if (this->stopped_)
        return;
    if (ec)
    {
        if (ec != asio::error::operation_aborted)
            std::cerr << "[net] accept failed: " << ec.message() << std::endl;
        this->DoAccept();
        return;
    }

    SessionOptions options;
    options.max_payload_bytes = this->config_.max_payload_bytes;
    options.data_queue_frames = this->config_.data_queue_frames;
    options.control_queue_frames = this->config_.control_queue_frames;

    auto session = std::make_shared<Session>(
        this->io_, std::move(socket), this->control_, options,
        [this](Session *closed)
        { this->OnSessionClosed(closed); });

    auto old = this->session_;
    this->session_ = session;
    this->publisher_->SetSession(session);
    if (old)
        old->Close();
    session->Start();
    std::cout << "[net] client connected: " << session->Remote() << std::endl;

    this->DoAccept();
}

void vuprs::net::AsioServer::OnSessionClosed(Session *session)
{
    if (this->session_.get() != session)
        return;
    this->publisher_->ResetSession(session);
    this->session_.reset();
    std::cout << "[net] client disconnected." << std::endl;
}
