#include "linux_session.h"

vuprs::LinuxSession::LinuxSession(int client_fd, const struct sockaddr_in& client_addr, const std::string &frameHeader, const std::string &frameTailer)
{
    this->client_fd = client_fd;
    this->client_addr = client_addr;
    this->running = false;
    this->frameHeader = frameHeader;
    this->frameTailer = frameTailer;
}

vuprs::LinuxSession::~LinuxSession()
{
    this->Stop();
}

bool vuprs::LinuxSession::SendData(const std::string &message)
{
    return vuprs::SocketSendData(this->client_fd, message.c_str(), message.length(), nullptr, nullptr);
}

void vuprs::LinuxSession::Start() 
{
    this->running = true;
    std::cout << "[session][" << this->GetClientInfo() << "] start receive loop."   << std::endl;
    this->SendData("[session] you are connected.");
    this->ReceiveLoop();
}

void vuprs::LinuxSession::Stop()
{
    /* Close client file descriptor */

    if (this->client_fd >= 0)
    {
        close(this->client_fd);
        this->client_fd = -1;
    }

    this->running = false;
}

bool vuprs::LinuxSession::IsRun() const 
{
    return this->running;
}

std::string vuprs::LinuxSession::GetClientInfo() const 
{
    char ip[INET_ADDRSTRLEN];

    /* Calculate IP & Port */

    inet_ntop(AF_INET, &this->client_addr.sin_addr, ip, INET_ADDRSTRLEN);

    return std::string(ip) + ":" + std::to_string(ntohs(this->client_addr.sin_port));
}

void vuprs::LinuxSession::SetMessageHandler(vuprs::SessionMessageHandler handler)
{
    this->messageHandler = std::move(handler);
}

void vuprs::LinuxSession::ReceiveLoop() 
{
    vuprs::SocketReceiveData data;  /* received data */
    
    while (this->running && this->client_fd >= 0) 
    {
        vuprs::SocketReceiveCommand(this->client_fd, this->frameTailer, &data);
        
        if (data.is_connect && data.receiveBytes > 0)
        {
            std::string message;

            if(vuprs::ParseMessageFromSocketData(data, this->frameHeader, this->frameTailer, &message))
            {
                /* Get message from client */
            
                if (this->messageHandler != nullptr)
                {
                    this->messageHandler(this->client_fd, this->client_addr, message);  /* Call user function */
                }
                else
                {
                    std::string response = this->DefaultMessageProcess(message);
                    this->SendData(response);
                }
            }
        }
        else if (!data.is_connect)  /* Connect shut down */
        {
            std::cout << "[session][" << this->GetClientInfo() << "] disconnected."  << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    running = false;

    std::cout << "[session][" << GetClientInfo() << "] client service end." << std::endl;
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

bool vuprs::SocketSendData(int client_fd, const char *buf, const uint64_t &sendLength, ssize_t *origin_ret, uint64_t *sendBytes)
{
    if (client_fd < 0 || !buf || sendLength == 0) 
    {
        if (origin_ret) *origin_ret = 1;
        if (sendBytes) *sendBytes = 0;
        return false;
    }

    ssize_t sendReturn;
    uint64_t currentSentBytes = 0;
    const uint64_t max_tries = 100;
    uint64_t tryCount = 0;
    bool timeoutFlag = false;
    
    while (currentSentBytes < sendLength && tryCount < max_tries) 
    {
        sendReturn = send(client_fd, buf + currentSentBytes, sendLength - currentSentBytes, 0);

        if (sendReturn > 0) 
        {
            currentSentBytes += sendReturn;
            tryCount = 0;
            continue;
        }
        else if (sendReturn == 0) 
        {
            break;
        }
        else 
        { // sendReturn < 0
            if (errno == EINTR) 
            {
                continue;
            }
            else if (errno == EWOULDBLOCK || errno == EAGAIN) 
            {
                tryCount++;
                usleep(100 * 1000); // 100ms
                continue;
            }
            else 
            {
                break;
            }
        }
    }

    bool success = (currentSentBytes == sendLength);
    
    if (origin_ret) *origin_ret = sendReturn;
    if (sendBytes) *sendBytes = currentSentBytes;
    
    return success;
}

void vuprs::SetSocketReceiveDataToDefault(vuprs::SocketReceiveData *data)
{
    data->is_connect = true;
    data->is_timeout = false;
    data->is_error = false;
    data->receiveBytes = 0;
    memset(data->buf, 0, sizeof(data->buf));
}

void vuprs::SocketReceiveCommand(int client_fd, const std::string &tailer, vuprs::SocketReceiveData *data)
{
    ssize_t recvReturn = 1;
    const auto timeout = std::chrono::milliseconds(50);
    auto start = std::chrono::steady_clock::now();

    vuprs::SetSocketReceiveDataToDefault(data);
    
    while (data->receiveBytes < __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__) 
    {
        
        if (std::chrono::steady_clock::now() - start > timeout) 
        {
            data->is_timeout = true;
            break;
        }
        
        recvReturn = recv(client_fd, data->buf + data->receiveBytes, __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__ - data->receiveBytes, 0);
        
        if (recvReturn > 0)  /* Successfully received */
        {
            data->receiveBytes += recvReturn;
            std::string_view receivedData(data->buf, data->receiveBytes);
            if (receivedData.find(tailer) != std::string_view::npos)
            {
                break;
            }
        }
        else if (recvReturn == 0)  /* Closed */
        {
            data->is_connect = false;
            break;
        }
        else  /* Error occurred */
        {
            if (errno == EINTR) continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
            data->is_error = true;
            break;
        }
    }
}

bool vuprs::SocketSendBuffer(int client_fd, const vuprs::AlignedBufferDMA &buffer)
{

}

bool vuprs::ParseMessageFromSocketData(const vuprs::SocketReceiveData &data, const std::string &header, const std::string &tailer, std::string *result)
{
    if (!result) return false;
    if (header.empty() || tailer.empty()) return false;
    
    std::string dataString(data.buf, data.receiveBytes);
    
    size_t headerPos = dataString.find(header);
    size_t tailerPos = dataString.find(tailer);
    
    if (headerPos != std::string::npos && tailerPos != std::string::npos && headerPos + header.length() < tailerPos)
    {
        size_t contentStart = headerPos + header.length();
        size_t contentLength = tailerPos - contentStart;
        
        *result = dataString.substr(contentStart, contentLength);
        return true;
    }
    
    return false;
}

bool vuprs::SocketSendFile(int client_fd, const std::string &filename)
{
    vuprs::AlignedBufferServer buffer;

    /* Load data from file */

    if (!buffer.from_file(filename))
    {
        return false;
    }

    /* Slice file */

    ssize_t reserveBytes, oneSendBytes, sendRet;
    uint64_t transferBytes = buffer.size(), sendCount = 0, sentBytes;
    uint64_t slices = (transferBytes % __SOCKET_SEND_PACKAGE_SIZE_BYTES__ == 0)? transferBytes / __SOCKET_SEND_PACKAGE_SIZE_BYTES__: 
                      transferBytes / __SOCKET_SEND_PACKAGE_SIZE_BYTES__ + 1;
    char* buffer_char = buffer.as<char>(), *startPointer;
    uint64_t tries = 0;

    /* Send data */

    reserveBytes = transferBytes;
    sendCount = 0;

    while(reserveBytes > 0)
    {
        /* pointer */

        startPointer = buffer_char + sendCount * __SOCKET_SEND_PACKAGE_SIZE_BYTES__;

        /* package bytes */

        if (reserveBytes >= __SOCKET_SEND_PACKAGE_SIZE_BYTES__)
        {
            oneSendBytes = __SOCKET_SEND_PACKAGE_SIZE_BYTES__;
        }
        else
        {
            oneSendBytes = reserveBytes;
        }

        /* send */

        if (vuprs::SocketSendData(client_fd, startPointer, oneSendBytes, &sendRet, &sentBytes))
        {
            sendCount++;
            reserveBytes -= oneSendBytes;
        }
        else
        {
            tries++;
        }

        /* break */

        if (reserveBytes <= 0) break;

        if (sendRet == 0) return false;  /* disconnect here */
        if (tries >= 100) return false;
    }

    buffer.release();
    
    return true;
}
