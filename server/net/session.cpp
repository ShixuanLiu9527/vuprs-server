#include "server/net/session.h"

#include <iostream>
#include <utility>

vuprs::net::Session::Session(asio::io_context &io,
                             asio::ip::tcp::socket socket,
                             std::shared_ptr<ControlWorker> worker,
                             SessionOptions options,
                             CloseHandler on_close)
    : strand_(asio::make_strand(io)),
      socket_(std::move(socket)),
      worker_(std::move(worker)),
      options_(options),
      on_close_(std::move(on_close))
{
    asio::error_code ec;
    this->socket_.set_option(asio::ip::tcp::no_delay(true), ec);
    const auto endpoint = this->socket_.remote_endpoint(ec);
    this->remote_ = ec ? "unknown" : (endpoint.address().to_string() + ":" + std::to_string(endpoint.port()));
}

vuprs::net::Session::~Session()
{
    this->closed_ = true;
    asio::error_code ec;
    this->socket_.close(ec);
}

void vuprs::net::Session::Start()
{
    auto self = this->shared_from_this();
    asio::post(this->strand_, [self]()
               { self->DoReadHeader(); });
}

void vuprs::net::Session::Close()
{
    if (this->closed_.load())
        return;
    /* Callers are either the session strand or the owning io thread. */
    auto self = this->shared_from_this(); /* keep alive while on_close_ may reset the owner's ptr */
    this->CloseOnStrand();
}

void vuprs::net::Session::DoReadHeader()
{
    if (this->closed_.load())
        return;
    auto self = this->shared_from_this();
    asio::async_read(this->socket_, asio::buffer(this->header_buf_),
                     asio::bind_executor(this->strand_, [self](const asio::error_code &ec, size_t bytes)
                                         { self->OnHeader(ec, bytes); }));
}

void vuprs::net::Session::OnHeader(const asio::error_code &ec, size_t bytes)
{
    if (this->closed_.load())
        return;
    if (ec)
    {
        if (ec == asio::error::eof || ec == asio::error::operation_aborted)
            this->CloseOnStrand();
        else
            this->Fail(ec, "read frame header");
        return;
    }
    if (bytes != this->header_buf_.size())
    {
        this->Fail(asio::error_code(), "short frame header");
        return;
    }
    std::string error;
    if (!DecodeFrameHeader(this->header_buf_.data(), this->header_buf_.size(), &this->current_header_, &error))
    {
        this->Fail(asio::error_code(), "invalid frame header: " + error);
        return;
    }
    if (this->current_header_.type != MessageType::CONTROL_REQUEST)
    {
        this->Fail(asio::error_code(), "client sent non-control frame: " + MessageTypeName(this->current_header_.type));
        return;
    }
    if (this->current_header_.payload_bytes > this->options_.max_payload_bytes)
    {
        this->Fail(asio::error_code(), "control payload too large");
        return;
    }
    this->payload_buf_.resize(this->current_header_.payload_bytes);
    this->DoReadPayload();
}

void vuprs::net::Session::DoReadPayload()
{
    if (this->closed_.load())
        return;
    if (this->payload_buf_.empty())
    {
        this->OnPayload(asio::error_code(), 0);
        return;
    }
    auto self = this->shared_from_this();
    asio::async_read(this->socket_, asio::buffer(this->payload_buf_),
                     asio::bind_executor(this->strand_, [self](const asio::error_code &ec, size_t bytes)
                                         { self->OnPayload(ec, bytes); }));
}

void vuprs::net::Session::OnPayload(const asio::error_code &ec, size_t bytes)
{
    if (this->closed_.load())
        return;
    if (ec)
    {
        if (ec == asio::error::eof || ec == asio::error::operation_aborted)
            this->CloseOnStrand();
        else
            this->Fail(ec, "read control payload");
        return;
    }
    if (bytes != this->payload_buf_.size())
    {
        this->Fail(asio::error_code(), "short control payload");
        return;
    }
    this->HandleControlRequest();
    this->DoReadHeader();
}

void vuprs::net::Session::HandleControlRequest()
{
    const uint32_t request_id = this->current_header_.request_id;
    nlohmann::json request;
    try
    {
        const std::string body(this->payload_buf_.begin(), this->payload_buf_.end());
        request = nlohmann::json::parse(body);
    }
    catch (...)
    {
        ControlResult result;
        result.ok = false;
        result.error = static_cast<int>(ControlError::INVALID_JSON);
        result.info = "invalid json";
        this->EnqueueControlResponse(request_id, "invalid", std::move(result));
        return;
    }
    if (!request.is_object() || !request.contains("cmd") || !request["cmd"].is_string())
    {
        ControlResult result;
        result.ok = false;
        result.error = static_cast<int>(ControlError::INVALID_JSON);
        result.info = "missing cmd";
        this->EnqueueControlResponse(request_id, "invalid", std::move(result));
        return;
    }
    const std::string cmd = request["cmd"].get<std::string>();
    nlohmann::json params = nlohmann::json::object();
    if (request.contains("params"))
    {
        if (!request["params"].is_object())
        {
            ControlResult result;
            result.ok = false;
            result.error = static_cast<int>(ControlError::INVALID_PARAMS);
            result.info = "params must be an object";
            this->EnqueueControlResponse(request_id, cmd, std::move(result));
            return;
        }
        params = request["params"];
    }
    if (!this->worker_)
    {
        ControlResult result;
        result.ok = false;
        result.error = static_cast<int>(ControlError::NOT_READY);
        result.info = "control worker unavailable";
        this->EnqueueControlResponse(request_id, cmd, std::move(result));
        return;
    }

    std::weak_ptr<Session> weak = this->weak_from_this();
    const bool submitted = this->worker_->Submit(
        request_id, cmd, std::move(params),
        [weak](uint32_t id, std::string completed_cmd, ControlResult result)
        {
            if (auto self = weak.lock())
            {
                asio::post(self->strand_,
                           [self, id, completed_cmd = std::move(completed_cmd), result = std::move(result)]() mutable
                           {
                               self->EnqueueControlResponse(id, completed_cmd, std::move(result));
                           });
            }
        });
    if (!submitted)
    {
        ControlResult result;
        result.ok = false;
        result.error = static_cast<int>(ControlError::BUSY);
        result.info = "control queue full";
        this->EnqueueControlResponse(request_id, cmd, std::move(result));
    }
}

void vuprs::net::Session::EnqueueControlResponse(uint32_t request_id, const std::string &cmd, ControlResult result)
{
    if (this->closed_.load())
        return;
    nlohmann::json response;
    response["ok"] = result.ok;
    response["cmd"] = cmd;
    response["error"] = result.error;
    response["info"] = result.info;
    response["params"] = result.params.is_object() ? result.params : nlohmann::json::object();
    OutFrame frame = MakeJsonFrame(MessageType::CONTROL_RESPONSE, request_id, response.dump());
    this->EnqueueControlFrame(std::move(frame));
}

void vuprs::net::Session::EnqueueControlFrame(OutFrame frame)
{
    if (this->closed_.load())
        return;
    if (this->control_tx_.size() >= this->options_.control_queue_frames)
    {
        this->Fail(asio::error_code(), "control response queue overflow");
        return;
    }
    this->control_tx_.push_back(std::move(frame));
    this->WriteNext();
}

void vuprs::net::Session::EnqueueData(OutFrame frame)
{
    if (this->closed_.load())
        return;
    auto self = this->shared_from_this();
    asio::post(this->strand_, [self, frame = std::move(frame)]() mutable
               { self->EnqueueDataOnStrand(std::move(frame)); });
}

void vuprs::net::Session::EnqueueDataOnStrand(OutFrame frame)
{
    if (this->closed_.load())
        return;
    while (this->data_tx_.size() >= this->options_.data_queue_frames)
    {
        this->data_tx_.pop_front();
        ++this->dropped_frames_;
    }
    this->data_tx_.push_back(std::move(frame));
    this->WriteNext();
}

void vuprs::net::Session::WriteNext()
{
    if (this->closed_.load() || this->writing_)
        return;
    if (!this->control_tx_.empty())
    {
        this->current_ = std::move(this->control_tx_.front());
        this->control_tx_.pop_front();
    }
    else if (!this->data_tx_.empty())
    {
        this->current_ = std::move(this->data_tx_.front());
        this->data_tx_.pop_front();
    }
    else
    {
        return;
    }
    this->writing_ = true;
    auto self = this->shared_from_this();
    auto buffers = this->BuildBuffers();
    asio::async_write(this->socket_, buffers,
                      asio::bind_executor(this->strand_, [self](const asio::error_code &ec, size_t bytes)
                                          { self->OnWrite(ec, bytes); }));
}

std::array<asio::const_buffer, 3> vuprs::net::Session::BuildBuffers() const
{
    std::array<asio::const_buffer, 3> buffers{
        asio::const_buffer(this->current_.header.data(), this->current_.header.size()),
        asio::const_buffer(),
        asio::const_buffer()};
    if (this->current_.bytes)
    {
        buffers[1] = asio::buffer(*this->current_.bytes);
    }
    else
    {
        buffers[1] = asio::const_buffer(this->current_.data_header.data(), this->current_.data_header.size());
        if (this->current_.bf_samples)
            buffers[2] = asio::buffer(this->current_.bf_samples->data(),
                                      this->current_.bf_samples->size() * sizeof(uint32_t));
        else if (this->current_.scan_values)
            buffers[2] = asio::buffer(this->current_.scan_values->data(),
                                      this->current_.scan_values->size() * sizeof(uint16_t));
    }
    return buffers;
}

void vuprs::net::Session::OnWrite(const asio::error_code &ec, size_t bytes)
{
    (void)bytes;
    if (this->closed_.load())
        return;
    if (ec)
    {
        if (ec == asio::error::eof || ec == asio::error::operation_aborted)
            this->CloseOnStrand();
        else
            this->Fail(ec, "write frame");
        return;
    }
    this->writing_ = false;
    this->WriteNext();
}

void vuprs::net::Session::Fail(const asio::error_code &ec, const std::string &where)
{
    if (this->closed_.load())
        return;
    std::cerr << "[net][" << this->remote_ << "] " << where;
    if (ec)
        std::cerr << ": " << ec.message();
    std::cerr << std::endl;
    this->CloseOnStrand();
}

void vuprs::net::Session::CloseOnStrand()
{
    if (this->closed_.exchange(true))
        return;
    this->control_tx_.clear();
    this->data_tx_.clear();
    asio::error_code ec;
    this->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    this->socket_.close(ec);
    if (this->on_close_)
    {
        auto handler = std::move(this->on_close_);
        this->on_close_ = nullptr;
        handler(this);
    }
}
