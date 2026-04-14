#include "socket_io_manager.h"

#include <errno.h>
#include <chrono>
#include <thread>

static constexpr int kSocketSendTimeoutMs = 500;
static constexpr int kSocketRecvTimeoutMs = 100;
static constexpr int kSendRetryMaximumCount = 3;

static bool SetSocketTimeoutOption(int fd, int option, int timeoutMs)
{
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv)) == 0;
}

static bool SendAllWithRetry(int fd, const char* data, size_t bytes)
{
    if (fd < 0 || data == nullptr || bytes == 0)
    {
        return false;
    }
    size_t totalSent = 0;
    int retryCount = 0;
    while (totalSent < bytes)
    {
        const size_t remaining = bytes - totalSent;
        ssize_t sent = send(fd, data + totalSent, remaining, 0);
        if (sent > 0)
        {
            totalSent += static_cast<size_t>(sent);
            retryCount = 0;
            continue;
        }
        if (sent == 0)
        {
            return false;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            retryCount++;
            if (retryCount > kSendRetryMaximumCount)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        return false;
    }
    return true;
}

vuprs::SocketIOManager::SocketIOManager(int client_fd, const sockaddr_in &client_addr)
{
    this->client_fd = client_fd;
    this->client_addr = client_addr;
    this->clientInformation = vuprs::ParseClientInformationFromSocketaddr(this->client_addr);

    SetSocketTimeoutOption(this->client_fd, SO_SNDTIMEO, kSocketSendTimeoutMs);
    SetSocketTimeoutOption(this->client_fd, SO_RCVTIMEO, kSocketRecvTimeoutMs);
}

vuprs::SocketIOManager::~SocketIOManager()
{
    this->CloseSocket();
}

std::string vuprs::SocketIOManager::ClientInformation() const
{
    return this->clientInformation;
}

bool vuprs::SocketIOManager::SendMessage(const std::string &message)
{
    if (message.empty())
    {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    return SendAllWithRetry(this->client_fd, message.data(), message.size());
}

bool vuprs::SocketIOManager::SendBuffer(const vuprs::AlignedBufferDMA &buffer)
{
    if (!buffer.is_allocated() || buffer.size() == 0)
    {
        return false;
    }

    const char* data_ptr = reinterpret_cast<const char*>(buffer.data());

    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    return SendAllWithRetry(this->client_fd, data_ptr, static_cast<size_t>(buffer.size()));
}

bool vuprs::SocketIOManager::SendBuffer(const std::vector<double> &buffer)
{
    if (buffer.empty()) 
    {
        return false;
    }

    const char* data_ptr = reinterpret_cast<const char*>(buffer.data());
    size_t bytes = buffer.size() * sizeof(double);
    
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    return SendAllWithRetry(this->client_fd, data_ptr, bytes);
}

void vuprs::SocketIOManager::ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data)
{
    if (data == nullptr)
    {
        return;
    }

    ssize_t recvReturn = 1;

    vuprs::SetSocketReceiveDataToDefault(data);

    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    if (this->client_fd < 0)
    {
        data->is_connect = false;
        data->is_error = true;
        return;
    }

    while (data->receiveBytes < __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__) 
    {
        recvReturn = recv(this->client_fd, data->buf + data->receiveBytes, __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__ - data->receiveBytes, 0);
        
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
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                data->is_timeout = true;
                break;
            }
            data->is_error = true;
            break;
        }
    }
}

void vuprs::SocketIOManager::CloseSocket()
{
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    if (this->client_fd >= 0)
    {
        shutdown(this->client_fd, SHUT_RDWR);
        close(this->client_fd);
        this->client_fd = -1;
    }
}

std::string vuprs::ParseClientInformationFromSocketaddr(const sockaddr_in &client_addr)
{
    char ip[INET_ADDRSTRLEN];

    /* Calculate IP & Port */

    inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);

    return std::string(ip) + ":" + std::to_string(ntohs(client_addr.sin_port));
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

void vuprs::SetSocketReceiveDataToDefault(vuprs::SocketReceiveData *data)
{
    data->is_connect = true;
    data->is_timeout = false;
    data->is_error = false;
    data->receiveBytes = 0;
    memset(data->buf, 0, sizeof(data->buf));
}
