#include <exception>
#include "server/net/control_worker.h"

vuprs::net::ControlWorker::ControlWorker(std::shared_ptr<IControlService> service, size_t max_queue)
    : service_(std::move(service)), max_queue_(max_queue == 0 ? 1 : max_queue)
{
}

vuprs::net::ControlWorker::~ControlWorker()
{
    this->Stop();
}

void vuprs::net::ControlWorker::Start()
{
    if (this->running_.exchange(true))
        return;
    this->thread_ = std::thread([this]()
                                { this->Loop(); });
}

void vuprs::net::ControlWorker::Stop()
{
    if (!this->running_.exchange(false))
        return;
    this->cv_.notify_all();
    if (this->thread_.joinable())
        this->thread_.join();
    {
        std::lock_guard<std::mutex> lock(this->mut_);
        this->queue_.clear();
    }
}

bool vuprs::net::ControlWorker::Submit(uint32_t request_id, std::string cmd, nlohmann::json params, Completion completion)
{
    {
        std::lock_guard<std::mutex> lock(this->mut_);
        if (!this->running_ || this->queue_.size() >= this->max_queue_)
            return false;
        Task task;
        task.request_id = request_id;
        task.cmd = std::move(cmd);
        task.params = std::move(params);
        task.completion = std::move(completion);
        this->queue_.push_back(std::move(task));
    }
    this->cv_.notify_one();
    return true;
}

void vuprs::net::ControlWorker::Loop()
{
    while (true)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(this->mut_);
            this->cv_.wait(lock, [this]()
                           { return !this->running_ || !this->queue_.empty(); });
            if (!this->running_)
                break;
            task = std::move(this->queue_.front());
            this->queue_.pop_front();
        }
        ControlResult result;
        try
        {
            result = this->service_->Execute(task.cmd, task.params);
        }
        catch (const std::exception &e)
        {
            result.ok = false;
            result.error = static_cast<int>(ControlError::INTERNAL);
            result.info = e.what();
        }
        catch (...)
        {
            result.ok = false;
            result.error = static_cast<int>(ControlError::INTERNAL);
            result.info = "unknown control error";
        }
        if (task.completion)
        {
            try
            {
                task.completion(task.request_id, std::move(task.cmd), std::move(result));
            }
            catch (...)
            {
                /* Completion normally posts to Asio; never let it kill the worker. */
            }
        }
    }
}
