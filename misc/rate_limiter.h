#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <chrono>

namespace vuprs
{
    /**
     * @brief Monotonic rate limiter based on steady_clock.
     *
     * @note Allow() returns true on the first call after construction or Reset(),
     * @note then at most once per [period].
     * @note Not thread-safe: use one instance from one thread.
     */
    class RateLimiter
    {
    private:
        std::chrono::steady_clock::duration period_; /* minimum interval between two allowed calls */
        std::chrono::steady_clock::time_point last_; /* time of the last allowed call */
        bool has_last_ = false;                      /* false: next Allow() passes immediately */

    public:
        template <typename Rep, typename Period>
        explicit RateLimiter(std::chrono::duration<Rep, Period> period)
            : period_(std::chrono::duration_cast<std::chrono::steady_clock::duration>(period))
        {
        }

        /**
         * @brief Try to acquire a slot.
         *
         * @retval true: allowed now (the internal timestamp is updated).
         * @retval false: too early, try again later.
         */
        bool Allow()
        {
            const auto now = std::chrono::steady_clock::now();
            if (!this->has_last_ || now - this->last_ >= this->period_)
            {
                this->last_ = now;
                this->has_last_ = true;
                return true;
            }
            return false;
        }

        /**
         * @brief Make the next Allow() pass immediately.
         */
        void Reset()
        {
            this->has_last_ = false;
        }
    };
}

#endif
