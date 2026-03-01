#include "linux_session.h"

vuprs::LinuxSession::LinuxSession(const std::string &frameHeader, const std::string &frameTailer)
{
    this->running = false;
    this->isIOManagerBind = false;
    this->frameHeader = frameHeader;
    this->frameTailer = frameTailer;
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
        manager = this->socketIOManager.lock();
    }
    if (manager == nullptr)
    {
        throw std::runtime_error("IO Manager is NULL.");
    }
    return manager->SendMessage(message);
}

void vuprs::LinuxSession::ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data)
{
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socketIOManager.lock();
    }
    if (manager == nullptr)
    {
        throw std::runtime_error("IO Manager is NULL.");
    }
    manager->ReceiveMessage(tailer, data);
}

std::string vuprs::LinuxSession::ClientInformation() const
{
    std::shared_ptr<vuprs::SocketIOManager> manager;
    {
        std::unique_lock<std::mutex> lock(this->mut);
        manager = this->socketIOManager.lock();
    }
    if (manager == nullptr)
    {
        throw std::runtime_error("IO Manager is NULL.");
    }
    return manager->ClientInformation();
}

void vuprs::LinuxSession::Start() 
{
    this->running = true;
    std::cout << "[session][" << this->ClientInformation() << "] start receive loop."   << std::endl;
    this->SendMessage("[session] you are connected.");
    this->ReceiveLoop();
}

void vuprs::LinuxSession::Stop()
{
    /* Close client file descriptor */

    this->running = false;
}

bool vuprs::LinuxSession::BindIOManager(std::shared_ptr<vuprs::SocketIOManager> ioManager)
{
    if (ioManager == nullptr)
    {
        throw std::runtime_error("Socket IO Manager is NULL.");
    }
    this->UnbindIOManager();
    {
        std::unique_lock<std::mutex> lock(this->mut);
        this->socketIOManager = ioManager;
    }
    this->isIOManagerBind = true;
    return true;
}

void vuprs::LinuxSession::UnbindIOManager()
{
    {
        std::unique_lock<std::mutex> lock(this->mut);
        this->socketIOManager.reset();
    }
    this->isIOManagerBind = false;
}

bool vuprs::LinuxSession::IsRun() const 
{
    return this->running;
}

void vuprs::LinuxSession::SetMessageHandler(vuprs::SessionMessageHandler handler)
{
    this->messageHandler = std::move(handler);
}

void vuprs::LinuxSession::ReceiveLoop() 
{
    vuprs::SocketReceiveData data;  /* received data */
    
    while (this->running)
    {

        this->ReceiveMessage(this->frameTailer, &data);
        
        if (data.is_connect && data.receiveBytes > 0)
        {
            std::string message;

            if(vuprs::ParseMessageFromSocketData(data, this->frameHeader, this->frameTailer, &message))
            {
                /* Get message from client */
            
                if (this->messageHandler != nullptr)
                {
                    this->messageHandler(this->socketIOManager, message);  /* Call user function */
                }
                else
                {
                    std::string response = this->DefaultMessageProcess(message);
                    this->SendMessage(response);
                }
            }
        }
        else if (!data.is_connect)  /* Connect shut down */
        {
            std::cout << "[session][" << this->ClientInformation() << "] disconnected."  << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    running = false;

    std::cout << "[session][" << this->ClientInformation() << "] client service end." << std::endl;
}

std::string vuprs::LinuxSession::DefaultMessageProcess(const std::string& message) 
{
    if (message == "hello") 
    {
        return "This is vuprs server.";
    }
    else if (message == "status")
    {
        return "server status: running";
    } 
    else if (message == "quit" || message == "exit") 
    {
        return "server quit.";
    } 
    else
    {
        return "echo: " + message;
    }
}
