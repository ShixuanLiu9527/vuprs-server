#include <algorithm>
#include <exception>
#include <iostream>
#include "misc/mt.h"

void vuprs::WaitForAllFutures(std::vector<std::future<void>> &futures)
{
    std::exception_ptr first_error;
    for (auto &future : futures)
    {
        try
        {
            future.get();
        }
        catch (...)
        {
            if (!first_error)
                first_error = std::current_exception();
        }
    }
    if (first_error)
        std::rethrow_exception(first_error);
}

vuprs::ThreadPool::ThreadPool(size_t num_threads) : stop(false)
{
    /* At least one worker; never exceed logical CPUs. */
    const size_t max_threads = std::max<size_t>(size_t{1}, std::thread::hardware_concurrency());
    num_threads = std::clamp<size_t>(num_threads, size_t{1}, max_threads);
    for (size_t i = 0; i < num_threads; ++i)
    {
        /* Create one thread */
        this->workers.emplace_back(
            [this]
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]
                                             { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    try
                    {
                        task(); /* Do task */
                    }
                    catch (const std::exception &e)
                    {
                        std::cout << "Task threw exception: " << e.what() << std::endl;
                    }
                    catch (...)
                    {
                        std::cout << "Task threw unknown exception." << std::endl;
                    }
                }
            });
    }
}

vuprs::ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        this->stop = true;
    }
    this->condition.notify_all();
    for (std::thread &worker : this->workers)
    {
        worker.join();
    }
}
