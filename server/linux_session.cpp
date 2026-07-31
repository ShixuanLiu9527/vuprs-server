#include "config.h"
#include "server/linux_session.h"
#include "logger/log_manager.h"

#define LINUX_SESSION_CPP__DEBUG_PRINT false /* print something @ debug mode */

vuprs::LinuxSession::LinuxSession(const std::string &frame_header, const std::string &frame_tailer)
{
    this->running = false;
    this->is_io_manager_bind = false;
    this->frame_header = frame_header;
    this->frame_tailer = frame_tailer;
}

vuprs::LinuxSession::~LinuxSession()
{
    this->Stop();
}

bool vuprs::LinuxSession::SendMessage(const std::string &message)
{
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socket_io_manager.lock();
    }
    PARAM_CHECK(manager != nullptr, "server", " in [LinuxSession::SendMessage] IO Manager is NULL.");
    return manager->SendMessage(message);
}

void vuprs::LinuxSession::ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data)
{
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socket_io_manager.lock();
    }
    PARAM_CHECK(manager != nullptr, "server", " in [LinuxSession::ReceiveMessage] IO Manager is NULL.");
    manager->ReceiveMessage(tailer, data);
}

std::string vuprs::LinuxSession::ClientInformation() const
{
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socket_io_manager.lock();
    }
    PARAM_CHECK(manager != nullptr, "server", " in [LinuxSession::ClientInformation] IO Manager is NULL.");
    return manager->ClientInformation();
}

void vuprs::LinuxSession::Start()
{
    this->running = true;
    std::cout << "[session][" << this->ClientInformation() << "] start receive loop." << std::endl;
    this->ReceiveLoop();
}

void vuprs::LinuxSession::Stop()
{
    /* Close client file descriptor */
    this->running = false;
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socket_io_manager.lock();
        this->socket_io_manager.reset();
    }
    if (manager != nullptr)
    {
        manager->CloseSocket();
    }
    this->is_io_manager_bind = false;
}

bool vuprs::LinuxSession::BindIOManager(std::shared_ptr<vuprs::SocketIOManager> io_manager)
{
    PARAM_CHECK(io_manager != nullptr, "server", " in [LinuxSession::BindIOManager] Socket IO Manager is NULL.");
    this->UnbindIOManager();
    {
        std::unique_lock<std::mutex> lock(this->mut);
        this->socket_io_manager = io_manager;
    }
    this->is_io_manager_bind = true;
    return true;
}

void vuprs::LinuxSession::UnbindIOManager()
{
    {
        std::unique_lock<std::mutex> lock(this->mut);
        this->socket_io_manager.reset();
    }
    this->is_io_manager_bind = false;
}

bool vuprs::LinuxSession::IsRun() const
{
    return this->running;
}

void vuprs::LinuxSession::SetMessageHandler(vuprs::SessionMessageHandler handler)
{
    this->message_handler = std::move(handler);
}

void vuprs::LinuxSession::ReceiveLoop()
{
    vuprs::SocketReceiveData data; /* received data */
    std::string client_info = "unknown-client";
    try
    {
        client_info = this->ClientInformation();
    }
    catch (...)
    {
    }
    while (this->running)
    {
        this->ReceiveMessage(this->frame_tailer, &data);
        if (data.is_connect && data.receive_bytes > 0)
        {
            std::string message;
            if (vuprs::CheckFrameFormat(data, this->frame_header, this->frame_tailer, &message))
            {
                /* Get message from client */
                if (this->message_handler != nullptr)
                {
#if DEBUG
                    std::cout << "[session][" << client_info << "] received message: " << message << std::endl;
#endif
                    this->message_handler(this->socket_io_manager, message); /* Call user function */
                }
                else
                {
                    std::string response = this->DefaultMessageProcess(message);
                    this->SendMessage(response);
                }
            }
        }
        else if (!data.is_connect) /* Connect shut down */
        {
            std::cout << "[session][" << client_info << "] disconnected." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    this->running = false;
    std::cout << "[session][" << client_info << "] client service end." << std::endl;
}

std::string vuprs::LinuxSession::DefaultMessageProcess(const std::string &message)
{
    if (message == "hello")
        return "This is vuprs server.";
    else if (message == "status")
        return "server status: running";
    else if (message == "quit" || message == "exit")
        return "server quit.";
    else
        return "echo: " + message;
}
