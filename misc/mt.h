#ifndef MT_H
#define MT_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "logger/check.h"

namespace vuprs
{
    /** @brief Wait for every future, then rethrow the first exception. */
    void WaitForAllFutures(std::vector<std::future<void>> &futures);

    class ThreadPool
    {
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;

    public:
        ThreadPool(size_t num_threads);
        ~ThreadPool();

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        template <class F, class... Args>
        std::future<typename std::result_of<F(Args...)>::type> enqueue(F &&f, Args &&...args)
        {
            using return_type = typename std::result_of<F(Args...)>::type;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<return_type> result = task->get_future();
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                RUNTIME_CHECK(!this->stop, "mt", " Enqueue on stopped ThreadPool");
                this->tasks.emplace([task]()
                                    { (*task)(); });
            }
            this->condition.notify_one();
            return result;
        }
    };
}

#endif
