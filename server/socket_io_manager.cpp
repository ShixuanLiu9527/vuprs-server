#include <errno.h>
#include <chrono>
#include <thread>
#include "server/socket_io_manager.h"

static constexpr int k_socket_send_timeout_ms = 500;
static constexpr int k_socket_recv_timeout_ms = 100;
static constexpr int k_send_retry_max_count = 3;

static bool SetSocketTimeoutOption(int fd, int option, int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv)) == 0;
}

bool vuprs::SendAllWithRetry(int fd, const char *data, size_t bytes)
{
    if (fd < 0 || data == nullptr || bytes == 0)
    {
        return false;
    }
    size_t total_sent = 0;
    int retry_count = 0;
    while (total_sent < bytes)
    {
        const size_t remaining = bytes - total_sent;
        ssize_t sent = send(fd, data + total_sent, remaining, 0);
        if (sent > 0)
        {
            total_sent += static_cast<size_t>(sent);
            retry_count = 0;
            continue;
        }
        if (sent == 0)
        {
            return false;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            retry_count++;
            if (retry_count > k_send_retry_max_count)
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
    this->client_info = vuprs::ParseClientInformationFromSocketaddr(this->client_addr);
    SetSocketTimeoutOption(this->client_fd, SO_SNDTIMEO, k_socket_send_timeout_ms);
    SetSocketTimeoutOption(this->client_fd, SO_RCVTIMEO, k_socket_recv_timeout_ms);
}

vuprs::SocketIOManager::~SocketIOManager()
{
    this->CloseSocket();
}

std::string vuprs::SocketIOManager::ClientInformation() const
{
    return this->client_info;
}

bool vuprs::SocketIOManager::SendMessage(const std::string &message)
{
    if (message.empty())
    {
        return true;
    }
    std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
    return vuprs::SendAllWithRetry(this->client_fd, message.data(), message.size());
}

bool vuprs::SocketIOManager::SendBuffer(const vuprs::AlignedBufferDMA &buffer)
{
    if (!buffer.is_allocated() || buffer.size() == 0)
    {
        return false;
    }
    const char *data_ptr = reinterpret_cast<const char *>(buffer.data());
    std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
    return vuprs::SendAllWithRetry(this->client_fd, data_ptr, static_cast<size_t>(buffer.size()));
}

void vuprs::SocketIOManager::ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data)
{
    if (data == nullptr)
    {
        return;
    }
    ssize_t recv_return = 1;
    vuprs::SetSocketReceiveDataToDefault(data);
    std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
    if (this->client_fd < 0)
    {
        data->is_connect = false;
        data->is_error = true;
        return;
    }
    while (data->receive_bytes < __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__)
    {
        recv_return = recv(this->client_fd,
                           data->buf + data->receive_bytes,
                           __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__ - data->receive_bytes,
                           0);
        if (recv_return > 0) /* Successfully received */
        {
            data->receive_bytes += recv_return;
            std::string_view received_data(data->buf, data->receive_bytes);
            if (received_data.find(tailer) != std::string_view::npos)
            {
                break;
            }
        }
        else if (recv_return == 0) /* Closed */
        {
            data->is_connect = false;
            break;
        }
        else /* Error occurred */
        {
            if (errno == EINTR)
                continue;
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
    std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
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

bool vuprs::CheckFrameFormat(const vuprs::SocketReceiveData &data, const std::string &header, const std::string &tailer, std::string *result)
{
    if (!result)
        return false;
    if (header.empty() || tailer.empty())
        return false;
    std::string data_string(data.buf, data.receive_bytes);
    data_string.erase(std::remove_if(data_string.begin(), data_string.end(),
                                     [](unsigned char ch)
                                     {
                                         return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
                                     }),
                      data_string.end());
    if (data_string.size() < header.size() + tailer.size())
        return false;
    if (data_string.compare(0, header.size(), header) != 0)
        return false;
    if (data_string.compare(data_string.size() - tailer.size(), tailer.size(), tailer) != 0)
        return false;
    *result = data_string; /* do not cut */
    return true;
}

void vuprs::SetSocketReceiveDataToDefault(vuprs::SocketReceiveData *data)
{
    data->is_connect = true;
    data->is_timeout = false;
    data->is_error = false;
    data->receive_bytes = 0;
    memset(data->buf, 0, sizeof(data->buf));
}
