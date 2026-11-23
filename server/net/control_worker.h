#ifndef VUPRS_NET_CONTROL_WORKER_H
#define VUPRS_NET_CONTROL_WORKER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "3rdparty/nlohmann/json.hpp"

namespace vuprs
{
    namespace net
    {
        enum class ControlError : int
        {
            OK = 0,
            INVALID_JSON = 1,
            UNKNOWN_COMMAND = 2,
            INVALID_PARAMS = 3,
            NOT_READY = 4,
            BUSY = 5,
            INTERNAL = 6
        };

        struct ControlResult
        {
            bool ok = false;
            int error = static_cast<int>(ControlError::INTERNAL);
            std::string info;
            nlohmann::json params = nlohmann::json::object();
        };

        class IControlService
        {
        public:
            virtual ~IControlService() = default;
            virtual ControlResult Execute(const std::string &cmd, const nlohmann::json &params) = 0;
        };

        /**
         * @brief Executes control commands on a dedicated thread, preserving order.
         */
        class ControlWorker
        {
        public:
            using Completion = std::function<void(uint32_t request_id, std::string cmd, ControlResult result)>;

            ControlWorker(std::shared_ptr<IControlService> service, size_t max_queue = 64);
            ~ControlWorker();

            ControlWorker(const ControlWorker &) = delete;
            ControlWorker &operator=(const ControlWorker &) = delete;

            void Start();
            void Stop();

            /**
             * @retval false if the worker is stopped or the queue is full.
             */
            bool Submit(uint32_t request_id, std::string cmd, nlohmann::json params, Completion completion);

        private:
            struct Task
            {
                uint32_t request_id = 0;
                std::string cmd;
                nlohmann::json params;
                Completion completion;
            };

            void Loop();

            std::shared_ptr<IControlService> service_;
            size_t max_queue_;
            std::atomic<bool> running_{false};
            std::thread thread_;
            std::mutex mut_;
            std::condition_variable cv_;
            std::deque<Task> queue_;
        };
    }
}

#endif
