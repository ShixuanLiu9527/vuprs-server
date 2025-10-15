#include "tcp_session.h"
#include "nerwork_exception.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

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
    return vuprs::SocketSendData(this->client_fd, message.c_str(), message.length());
}

void vuprs::TcpSession::start() 
{
    this->running = true;
    std::cout << "[session] start with client [" << this->getClientInfo() << "]."   << std::endl;

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
    uint64_t tryCount = 0;
    ssize_t recvReturn;
    bool recvStatus = false;
    
    while (this->running && this->client_fd >= 0) 
    {
        recvStatus = vuprs::SocketRecvData(this->client_fd, buffer, __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__, &recvReturn);
        
        if (recvStatus > 0)
        {
            /* Get message from client */

            buffer[static_cast<uint64_t>(recvReturn)] = '\0';
            std::string message(buffer);
            
            std::cout << "[session] received data from client [" << getClientInfo() << "]: " << message << std::endl;
            
            if (this->messageHandler != nullptr)
            {
                this->messageHandler(this->client_fd, this->client_addr, message);  /* Call user function */
            }
            else
            {
                std::string response = this->DefaultMessageProcess(message);
                vuprs::SocketSendData(this->client_fd, response.c_str(), response.length());
            }
            if (message == "quit" || message == "exit")
            {
                break;
            }
        }
        else if (recvReturn == 0)  /* Connect shut down */
        {
            std::cout << "[session] client disconnected. [" << getClientInfo() << "]"  << std::endl;
            break;
        }
        else
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)  /* wait */
            {
                std::cerr << "receive error. [" << strerror(errno) << "]"  << std::endl;
                break;
            }
        }

        if (tryCount >= __SOCKET_TIMEOUT_MAXIMUM_ITERATION_COUNT__)
        {
            break;
        }

        tryCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    running = false;

    std::cout << "[session] client service end. [" << getClientInfo() << "]" << std::endl;
}

std::string vuprs::TcpSession::DefaultMessageProcess(const std::string& message) {
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

bool vuprs::SocketSendData(int fd, const char *buf, const uint64_t &sendLength, ssize_t *origin_ret)
{
    uint64_t sentBytes = 0, tryCount = 0;
    ssize_t sendReturn = 0;
    bool retValue = false;

    if (fd < 0 || !buf)
    {
        return false;
    }

    while(true)
    {
        sendReturn = send(fd, buf + sentBytes, sendLength - sentBytes, 0);

        if (sendReturn < 0)
        {
            if (errno == EWOULDBLOCK)
            {
                continue;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                retValue = false;
                break;
            }
        }
        else if (sendReturn == 0)
        {
            retValue = false;
            break;
        }
        if (tryCount > __SOCKET_TIMEOUT_MAXIMUM_ITERATION_COUNT__)
        {
            retValue = false;
            break;
        }

        sentBytes += sendReturn;

        if (sentBytes >= sendLength)
        {
            retValue = true;
            break;
        }

        tryCount++;

#ifdef _WIN32
        Sleep(1);
#else
        usleep(1);
#endif

    }

    if (origin_ret)
    {
        (*origin_ret) = sendReturn;
    }

    return retValue;
}

bool vuprs::SocketRecvData(int fd, char* buf, const uint64_t &recvLength, ssize_t *origin_ret)
{
    uint64_t receivedBytes = 0, tryCount = 0;
    ssize_t recvReturn = 0;
    bool retValue = false;

    if (fd < 0 || !buf)
    {
        return false;
    }

    while (true)
    {
        recvReturn = recv(fd, buf + receivedBytes, recvLength - receivedBytes, 0);

        if (recvReturn < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                continue;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                retValue = false;
                break;
            }
        }
        else if (recvReturn == 0)
        {
            retValue = false;
            break;
        }

        if (tryCount > __SOCKET_TIMEOUT_MAXIMUM_ITERATION_COUNT__)
        {
            retValue = false;
            break;
        }

        receivedBytes += recvReturn;

        if (receivedBytes >= recvLength)
        {
            retValue = true;
            break;
        }

        tryCount++;

#ifdef _WIN32
        Sleep(1);
#else
        usleep(1);
#endif

    }

    if (origin_ret)
    {
        (*origin_ret) = recvReturn;
    }

    return retValue;
}
