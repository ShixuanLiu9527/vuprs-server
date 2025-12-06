#include "protocol.h"

bool WaitForWritable(int fd, int timeout_ms);

vuprs::NetworkProtocolMessage::NetworkProtocolMessage()
{

}

vuprs::NetworkProtocolMessage::~NetworkProtocolMessage()
{
    this->clear();
}

void vuprs::NetworkProtocolMessage::clear()
{
    this->data = nullptr;
    this->dataSize = 0;

    this->versionSet = false;
    this->typeSet = false;
    this->infoSet = false;
    this->statusSet = false;
    this->set = false;
}

void vuprs::NetworkProtocolMessage::SetVersion(uint8_t version)
{
    if (IS_VUPRS_NETWORK_PROTOCOL__VERSION(version)) this->version = version;
    else throw std::runtime_error("Invalid version: " + std::to_string(version));
    this->versionSet = true;
    this->UpdateStatus();
}

void vuprs::NetworkProtocolMessage::SetType(uint8_t type)
{
    if (IS_VUPRS_NETWORK_PROTOCOL__TYPE(type)) this->type = type;
    else throw std::runtime_error("Invalid type: " + std::to_string(type));
    this->typeSet = true;
    this->UpdateStatus();
}

void vuprs::NetworkProtocolMessage::SetInfo(uint8_t info)
{
    if (IS_VUPRS_NEWWORK_PROTOCOL__INFO(info)) this->info = info;
    else throw std::runtime_error("Invalid info: " + std::to_string(info));
    this->infoSet = true;
    this->UpdateStatus();
}

void vuprs::NetworkProtocolMessage::SetStatus(uint8_t status)
{
    if (IS_VUPRS_NETWORK_PROTOCOL__STATUS(status)) this->status = status;
    else throw std::runtime_error("Invalid status: " + std::to_string(status));
    this->statusSet = true;
    this->UpdateStatus();
}

void vuprs::NetworkProtocolMessage::SetData(const void *data, uint32_t size)
{
    this->data = data;
    this->dataSize = size;
}

void vuprs::NetworkProtocolMessage::GenerateHeader(uint32_t *firstWord, uint32_t *secondWord) const
{
    if (!this->set) throw std::runtime_error("Config not complete.");
    if (firstWord == nullptr || secondWord == nullptr) throw std::runtime_error("Output pointers cannot be null.");
    std::vector<uint8_t> bytes(8);
    std::fill(bytes.begin(), bytes.end(), 0);
    bytes[__VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__VERSION] = this->version;
    bytes[__VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__TYPE] = this->type;
    bytes[__VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__INFO] = this->info;
    bytes[__VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__STATUS] = this->status;
    
    *firstWord = (uint32_t)bytes[0] | 
                 (uint32_t)((uint32_t)bytes[1] << 8) | 
                 (uint32_t)((uint32_t)bytes[2] << 16) | 
                 (uint32_t)((uint32_t)bytes[3] << 24);
    *secondWord = (uint32_t)bytes[4] | 
                  (uint32_t)((uint32_t)bytes[5] << 8) | 
                  (uint32_t)((uint32_t)bytes[6] << 16) | 
                  (uint32_t)((uint32_t)bytes[7] << 24);
}

void vuprs::NetworkProtocolMessage::UpdateStatus()
{
    this->set = this->version && this->typeSet && this->infoSet && this->statusSet;
}

const uint8_t* vuprs::NetworkProtocolMessage::GetData() const 
{
    return this->data;
}

uint32_t vuprs::NetworkProtocolMessage::GetDataSize() const 
{
    return this->dataSize;
}

void vuprs::NetworkProtocolMessage::Serialize()
{
    if (!this->set)
    {
        throw std::runtime_error("Config not complete.");
    }

    std::vector<uint32_t> header(4);
    uint32_t dataPointerFromStart = 0, messageSize = 0, alignedDataSize = 0;
    uint32_t startMagic = htonl(__VUPRS_NETWORK_PROTOCOL__MAGIC__START_MAGIC), endMagic = htonl(__VUPRS_NETWORK_PROTOCOL__MAGIC__END_MAGIC);

    this->serializeData.clear();

    /* Calculate message size in bytes */

    messageSize += 2 * sizeof(uint32_t);  /* start & end magic */
    messageSize += __VUPRS_NETWORK_PROTOCOL__HEADER_SIZE;  /* data header (4 words, 16 bytes) */

    if (this->data != nullptr && this->dataSize != 0)  /* calculate aligned size */
    {
        alignedDataSize = (this->dataSize % 4U == 0)? this->dataSize: static_cast<uint32_t>((this->dataSize / 4U + 1) * 4U);
    }

    messageSize += alignedDataSize;

    this->serializeData.resize(messageSize);
    
    /* Generate the start magic */

    dataPointerFromStart = 0;

    memcpy(this->serializeData.data() + dataPointerFromStart, &startMagic, sizeof(uint32_t));

    dataPointerFromStart += 4;

    /* Generate the 4 words in header */

    this->GenerateHeader(&header[0], &header[1]);
    if (this->data == nullptr || this->dataSize == 0)
    {
        header[2] = 0;
        header[3] = 0;
    }
    else
    {
        header[2] = htonl(this->dataSize);
        header[3] = htonl(alignedDataSize);
    }
    header[0] = htonl(header[0]);
    header[1] = htonl(header[1]);
    
    memcpy(this->serializeData.data() + dataPointerFromStart, header.data(), header.size() * sizeof(uint32_t));

    dataPointerFromStart += header.size() * sizeof(uint32_t);

    /* Generate data block */

    if (this->data != nullptr && this->dataSize > 0)
    {
        memcpy(this->serializeData.data() + dataPointerFromStart, this->data, this->dataSize);

        if (alignedDataSize > this->dataSize)
        {
            uint32_t paddingSize = alignedDataSize - this->dataSize;
            uint8_t* paddingStart = this->serializeData.data() + dataPointerFromStart + this->dataSize;
            memset(paddingStart, 0, paddingSize);
        }

        dataPointerFromStart += alignedDataSize;
    }

    /* Generate end magic */

    memcpy(this->serializeData.data() + dataPointerFromStart, &endMagic, sizeof(uint32_t));
}

bool WaitForWritable(int fd, int timeout_ms) 
{
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    return select(fd + 1, NULL, &write_fds, NULL, &timeout) > 0;
}

bool vuprs::NetworkProtocolMessage::SendToSocket(int client_fd, uint32_t timeout_ms)
{
    if (client_fd < 0 || !this->set) 
    {
        return false;
    }

    this->Serialize();
    
    const uint8_t* buffer = this->serializeData.data();
    const uint32_t totalSize = static_cast<uint32_t>(this->serializeData.size());
    uint32_t sentBytes = 0;
    
    auto startTime = std::chrono::steady_clock::now();
    
    while (sentBytes < totalSize) 
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
            
        if (elapsed > timeout_ms)
        {
            break;
        }
        
        ssize_t result = send(client_fd, buffer + sentBytes, totalSize - sentBytes, 0);
        
        if (result > 0) 
        {
            sentBytes += static_cast<uint32_t>(result);
        }
        else if (result == 0) 
        {
            break;
        }
        else 
        {
            if (errno == EINTR) 
            {
                continue;
            }
            else if (errno == EWOULDBLOCK || errno == EAGAIN) 
            {
                int remaining_ms = timeout_ms - static_cast<int>(elapsed);
                if (remaining_ms <= 0 || !WaitForWritable(client_fd, remaining_ms)) 
                {
                    break;
                }
                continue;
            }
            else 
            {
                break;
            }
        }
    }
    
    return (sentBytes == totalSize);
}

bool vuprs::NetworkProtocolMessage::SendToSocket(int client_fd)
{
    if (client_fd < 0) 
    {
        return false;
    }

    if (!this->set)
    {
        throw std::runtime_error("Config not complete.");
    }

    uint32_t currentSentBytes = 0, totalSendLength = 0, tryCount = 0;
    int sendReturn = 0;

    this->Serialize();

    totalSendLength = this->serializeData.size();
    
    while (currentSentBytes < totalSendLength && tryCount < __VUPRS_NETWORK_PROTOCOL__SENDING_MAX_TRIES) 
    {
        sendReturn = send(client_fd, this->serializeData.data() + currentSentBytes, totalSendLength - currentSentBytes, 0);

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
        else  // sendReturn < 0
        {
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
    
    return success;
}
