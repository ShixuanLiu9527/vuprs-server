#ifndef VUPRS_NET_SESSION_H
#define VUPRS_NET_SESSION_H

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "asio.hpp"
#include "server/net/control_worker.h"
#include "server/net/frame.h"

namespace vuprs
{
    namespace net
    {
        struct SessionOptions
        {
            size_t max_payload_bytes = MAX_PAYLOAD_BYTES;
            size_t data_queue_frames = 64;
            size_t control_queue_frames = 16;
        };

        /**
         * @brief Single-client TCP session.
         *
         * Reads CONTROL_REQUEST frames on the socket; writes CONTROL_RESPONSE
         * and data frames through one writer. Control responses have priority.
         */
        class Session : public std::enable_shared_from_this<Session>
        {
        public:
            using CloseHandler = std::function<void(Session *)>;

            Session(asio::io_context &io,
                    asio::ip::tcp::socket socket,
                    std::shared_ptr<ControlWorker> worker,
                    SessionOptions options,
                    CloseHandler on_close);
            ~Session();

            Session(const Session &) = delete;
            Session &operator=(const Session &) = delete;

            void Start();
            void Close();

            /** Thread-safe; posts the frame to the session strand. */
            void EnqueueData(OutFrame frame);

            bool IsOpen() const { return !this->closed_.load(); }
            uint64_t DroppedFrames() const { return this->dropped_frames_.load(); }
            std::string Remote() const { return this->remote_; }

        private:
            using Strand = asio::strand<asio::io_context::executor_type>;

            void DoReadHeader();
            void OnHeader(const asio::error_code &ec, size_t bytes);
            void DoReadPayload();
            void OnPayload(const asio::error_code &ec, size_t bytes);
            void HandleControlRequest();

            void EnqueueControlResponse(uint32_t request_id, const std::string &cmd, ControlResult result);
            void EnqueueControlFrame(OutFrame frame);
            void EnqueueDataOnStrand(OutFrame frame);
            void WriteNext();
            void OnWrite(const asio::error_code &ec, size_t bytes);
            std::array<asio::const_buffer, 3> BuildBuffers() const;

            void Fail(const asio::error_code &ec, const std::string &where);
            void CloseOnStrand();

            Strand strand_;
            asio::ip::tcp::socket socket_;
            std::shared_ptr<ControlWorker> worker_;
            SessionOptions options_;
            CloseHandler on_close_;
            std::string remote_;

            std::array<uint8_t, FRAME_HEADER_SIZE> header_buf_{};
            FrameHeader current_header_;
            std::vector<uint8_t> payload_buf_;

            std::deque<OutFrame> control_tx_;
            std::deque<OutFrame> data_tx_;
            OutFrame current_;
            bool writing_ = false;

            std::atomic<bool> closed_{false};
            std::atomic<uint64_t> dropped_frames_{0};
        };
    }
}

#endif
