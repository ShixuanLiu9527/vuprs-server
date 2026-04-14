#include "socket_io_manager.h"

vuprs::SocketIOManager::SocketIOManager(int client_fd, const sockaddr_in &client_addr)
{
    this->client_fd = client_fd;
    this->client_addr = client_addr;
    this->clientInformation = vuprs::ParseClientInformationFromSocketaddr(this->client_addr);
}

std::string vuprs::SocketIOManager::ClientInformation() const
{
    return this->clientInformation;
}

bool vuprs::SocketIOManager::SendMessage(const std::string &message)
{
    const char* buffer = message.c_str();
    size_t total_sent = 0;
    size_t remaining = message.size();
    
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    while (remaining > 0) 
    {
        ssize_t sent = send(client_fd, buffer + total_sent, remaining, 0);

        if (sent <= 0) 
        {
            return false;
        }

        total_sent += sent;
        remaining -= sent;
    }

    return true;
}

bool vuprs::SocketIOManager::SendBuffer(const vuprs::AlignedBufferDMA &buffer)
{

}

bool vuprs::SocketIOManager::SendBuffer(const std::vector<double> &buffer)
{
    if (buffer.empty()) 
    {
        return false;
    }

    const char* data_ptr = reinterpret_cast<const char*>(buffer.data());
    size_t total_sent = 0;
    size_t remaining = buffer.size() * sizeof(double);
    
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    while (remaining > 0) 
    {
        ssize_t sent = send(this->client_fd, data_ptr + total_sent, remaining, 0);

        if (sent <= 0) 
        {
            return false;
        }

        total_sent += sent;
        remaining -= sent;
    }

    return true;
}

void vuprs::SocketIOManager::ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data)
{
    ssize_t recvReturn = 1;
    const auto timeout = std::chrono::milliseconds(50);
    auto start = std::chrono::steady_clock::now();

    vuprs::SetSocketReceiveDataToDefault(data);

    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    while (data->receiveBytes < __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__) 
    {
        if (std::chrono::steady_clock::now() - start > timeout) 
        {
            data->is_timeout = true;
            break;
        }
        
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
            if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
            data->is_error = true;
            break;
        }
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
