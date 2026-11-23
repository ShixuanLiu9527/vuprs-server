#ifndef VUPRS_NET_FRAME_PUBLISHER_H
#define VUPRS_NET_FRAME_PUBLISHER_H

#include <memory>
#include <mutex>
#include "server/net/frame.h"

namespace vuprs
{
    namespace net
    {
        class Session;

        /**
         * @brief Thread-safe bridge from DSP/mock threads to the active session.
         */
        class FramePublisher
        {
        public:
            void SetSession(const std::shared_ptr<Session> &session);
            void ResetSession(Session *expected = nullptr);
            bool HasSession() const;

            /** Posts the frame to the session; drops it if no client is connected. */
            void Publish(OutFrame frame);

        private:
            mutable std::mutex mut_;
            std::weak_ptr<Session> session_;
        };
    }
}

#endif
