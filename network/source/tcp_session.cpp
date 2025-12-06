#include "tcp_session.h"

vuprs::TcpSession::TcpSession(int client_fd, const struct sockaddr_in& client_addr)
{
    this->client_fd = client_fd;
    this->client_addr = client_addr;
    this->running = false;
}

vuprs::TcpSession::~TcpSession()
{
    this->stop();
}

vuprs::TcpSession::TcpSession(TcpSession&& other) noexcept
{
    this->client_fd = other.client_fd;
    this->client_addr = other.client_addr;
    this->running = other.running;
    this->messageHandler = std::move(other.messageHandler);

    other.client_fd = -1;
    other.running = false;
}

vuprs::TcpSession& vuprs::TcpSession::operator=(TcpSession&& other) noexcept 
{
    if (this != &other)
    {
        this->stop();
        this->client_fd = other.client_fd;
        this->client_addr = other.client_addr;
        this->running = other.running;
        this->messageHandler = std::move(other.messageHandler);
        
        other.client_fd = -1;
        other.running = false;
    }
    return *this;
}

bool vuprs::TcpSession::sendData(const std::string &message)
{
    return vuprs::SocketSendData(this->client_fd, message.c_str(), message.length(), nullptr, nullptr);
}

void vuprs::TcpSession::start() 
{
    this->running = true;
    std::cout << "[session][" << this->getClientInfo() << "] start receive loop."   << std::endl;
    this->sendData("[session] you are connected.");
    this->receiveLoop();
}

void vuprs::TcpSession::stop()
{
    /* Close client file descriptor */

    if (this->client_fd >= 0)
    {
        close(this->client_fd);
        this->client_fd = -1;
    }

    this->running = false;
}

bool vuprs::TcpSession::isRunning() const 
{
    return this->running;
}

std::string vuprs::TcpSession::getClientInfo() const 
{
    char ip[INET_ADDRSTRLEN];

    /* Calculate IP & Port */

    inet_ntop(AF_INET, &this->client_addr.sin_addr, ip, INET_ADDRSTRLEN);

    return std::string(ip) + ":" + std::to_string(ntohs(this->client_addr.sin_port));
}

void vuprs::TcpSession::setMessageHandler(SessionMessageHandler handler)
{
    this->messageHandler = std::move(handler);
}

void vuprs::TcpSession::receiveLoop() 
{
    char buffer[__SOCKET_RECEIVE_BUFFER_SIZE_BYTES__];
    uint64_t tryCount = 0, recvBytes = 0;
    ssize_t recvReturn;
    bool recvStatus = false;
    
    while (this->running && this->client_fd >= 0) 
    {
        recvStatus = vuprs::SocketRecvData(this->client_fd, buffer, __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__, &recvReturn, &recvBytes);
        
        if (recvStatus)
        {
            /* Get message from client */

            buffer[static_cast<uint64_t>(recvBytes)] = '\0';
            std::string message(buffer);
            
            std::cout << "[session][" << this->getClientInfo() << "] received data from client: " << message << std::endl;
            
            if (this->messageHandler != nullptr)
            {
                this->messageHandler(this->client_fd, this->client_addr, message);  /* Call user function */
            }
            else
            {
                std::string response = this->DefaultMessageProcess(message);
                this->sendData(response);
            }
            if (message == "quit" || message == "exit")
            {
                break;
            }
        }
        else if (recvReturn == 0)  /* Connect shut down */
        {
            std::cout << "[session][" << this->getClientInfo() << "] disconnected."  << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    running = false;

    std::cout << "[session][" << getClientInfo() << "] client service end." << std::endl;
}

std::string vuprs::TcpSession::DefaultMessageProcess(const std::string& message) 
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

bool waitSocketReadable(int fd, int timeout_ms) 
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(fd, &readfds)) 
    {
        return true;
    }
    return false;
}

bool vuprs::SocketRecvData(int client_fd, char* buf, const uint64_t &max_recvLength, ssize_t *origin_ret, uint64_t *recvBytes)
{
    uint64_t receivedBytes = 0;
    ssize_t recvReturn = 1;
    const auto timeout = std::chrono::seconds(5);
    auto start = std::chrono::steady_clock::now();
    bool timeoutFlag = false;
    
    while (receivedBytes < max_recvLength) 
    {
        
        if (std::chrono::steady_clock::now() - start > timeout) 
        {
            timeoutFlag = true;
            break;
        }
        
        if (!waitSocketReadable(client_fd, 100)) continue;
        
        recvReturn = recv(client_fd, buf + receivedBytes, max_recvLength - receivedBytes, 0);
        
        if (recvReturn > 0) 
        {
            receivedBytes += recvReturn;
        }
        else if (recvReturn == 0) 
        {
            break;
        }
        else 
        {
            if (errno == EINTR) continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
            break;
        }
        
        if (receivedBytes > 0) 
        {
            break;
        }
    }
    
    if (timeoutFlag)
    {
        if (origin_ret) *origin_ret = 1;  /* not disconnect when timeout */
    }
    else
    {
        if (origin_ret) *origin_ret = recvReturn;
    }
    if (recvBytes) *recvBytes = receivedBytes;
    
    return receivedBytes > 0;
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
