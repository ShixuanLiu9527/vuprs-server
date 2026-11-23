#include "server/net/frame_publisher.h"

#include <utility>
#include "server/net/session.h"

void vuprs::net::FramePublisher::SetSession(const std::shared_ptr<Session> &session)
{
    std::lock_guard<std::mutex> lock(this->mut_);
    this->session_ = session;
}

void vuprs::net::FramePublisher::ResetSession(Session *expected)
{
    std::lock_guard<std::mutex> lock(this->mut_);
    if (expected == nullptr || this->session_.lock().get() == expected)
        this->session_.reset();
}

bool vuprs::net::FramePublisher::HasSession() const
{
    std::lock_guard<std::mutex> lock(this->mut_);
    return !this->session_.expired();
}

void vuprs::net::FramePublisher::Publish(OutFrame frame)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(this->mut_);
        session = this->session_.lock();
    }
    if (session)
        session->EnqueueData(std::move(frame));
}
