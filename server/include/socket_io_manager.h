#ifndef SOCKET_IO_MANAGER_H
#define SOCKET_IO_MANAGER_H

#include <mutex>
#include <memory>
#include <cstring>

#ifdef __linux__
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif

#include "aligned_buffer.h"

#define __SOCKET_TIMEOUT_MAXIMUM_ITERATION_COUNT__   20
#define __SOCKET_RECEIVE_BUFFER_SIZE_BYTES__         1024UL
#define __SOCKET_SEND_PACKAGE_SIZE_BYTES__           1024UL

namespace vuprs
{
    struct SocketReceiveData
    {
        char buf[__SOCKET_RECEIVE_BUFFER_SIZE_BYTES__];  /* reserve buffer */
        ssize_t receiveBytes;  /* received bytes, 0: receive none, >0 receive bytes */
        bool is_connect;  /* true: client is connect, false: client is disconnect */
        bool is_timeout;  /* true: timeout, false: no timeout */
        bool is_error;  /* true: error occurred, false: no error */
    };

    void SetSocketReceiveDataToDefault(vuprs::SocketReceiveData *data);

    bool SendAllWithRetry(int fd, const char* data, size_t bytes);

    /**
     * @brief Socket IO Manager.
     * 
     * @note Thread safety.
     */
    class SocketIOManager
    {
        private:

            int client_fd;
            struct sockaddr_in client_addr;
            std::string clientInformation;

            mutable std::mutex mut;

        public:

            SocketIOManager(int client_fd, const sockaddr_in &client_addr);
            ~SocketIOManager();

            std::string ClientInformation() const;

            /**
             * @brief Socket send message.
             * 
             * @param message message to sent.
             * 
             * @retval true: success.
             * @retval false: failed (error or disconnected).
             */
            bool SendMessage(const std::string &message);

            bool SendBuffer(const vuprs::AlignedBufferDMA &buffer);

            template<typename T>
            bool SendBuffer(const std::vector<T> &buffer)
            {
                if (buffer.empty()) 
                {
                    return false;
                }
            
                const char* data_ptr = reinterpret_cast<const char*>(buffer.data());
                size_t bytes = buffer.size() * sizeof(T);

                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
            
                return vuprs::SendAllWithRetry(this->client_fd, data_ptr, bytes);
            }

            /**
             * @brief Socket receive message.
             * 
             * @param tailer frame tailer.
             * @param data output data.
             */
            void ReceiveMessage(const std::string &tailer, vuprs::SocketReceiveData *data);

            void CloseSocket();
    };

    /**
     * @brief Parse client information from struct sockaddr_in
     * 
     * @retval "{ip}:{port}"
     */
    std::string ParseClientInformationFromSocketaddr(const sockaddr_in &client_addr);

    /**
     * @brief Check frame format.
     * 
     * @note Check if the received data has correct frame format (header & tailer).
     * 
     * @retval true: if frame format is correct, and output the data string (including header & tailer) to result;
     * @retval false: otherwise, and result is unchanged.
     */
    bool CheckFrameFormat(const vuprs::SocketReceiveData &data, const std::string &header, const std::string &tailer, std::string *result);
}

#endif
