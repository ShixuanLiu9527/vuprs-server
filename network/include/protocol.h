#ifndef VUPRS_PROTOCOL_H
#define VUPRS_PROTOCOL_H

#include <stdint.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <arpa/inet.h>

/**
 * @brief Version.
 */

#define VUPRS_NETWORK_PROTOCOL__VERSION__V1                               (uint8_t)0x00

#define IS_VUPRS_NETWORK_PROTOCOL__VERSION(VAL) \
(VAL == VUPRS_NETWORK_PROTOCOL__VERSION__V1)

/**
 * @brief Type.
 */

#define VUPRS_NETWORK_PROTOCOL__TYPE__OPERATING                           (uint8_t)0x00  /* card to host & host to card */
#define VUPRS_NETWORK_PROTOCOL__TYPE__OPERATING_STATUS                    (uint8_t)0x01  /* card to host & host to card */
#define VUPRS_NETWORK_PROTOCOL__TYPE__DATA                                (uint8_t)0x02  /* card to host */

#define IS_VUPRS_NETWORK_PROTOCOL__TYPE(VAL) \
(VAL == VUPRS_NETWORK_PROTOCOL__TYPE__OPERATING || \
 VAL == VUPRS_NETWORK_PROTOCOL__TYPE__OPERATING_STATUS || \
 VAL == VUPRS_NETWORK_PROTOCOL__TYPE__DATA)

/**
 * @brief Information.
 */

#define VUPRS_NETWORK_PROTOCOL__INFO__OPERATING_STATUS__OPERATING_SUCCESS (uint8_t)0x00
#define VUPRS_NETWORK_PROTOCOL__INFO__OPERATING_STATUS__OPERATION_FAILED  (uint8_t)0x01

#define VUPRS_NETWORK_PROTOCOL__INFO__OPERATING__SET_DATA_SIZE            (uint8_t)0x00  /* card to host */
#define VUPRS_NETWORK_PROTOCOL__INFO__OPERATING__SET_SAMPLING_FREQUENCY   (uint8_t)0x00  /* host to card */
#define VUPRS_NETWORK_PROTOCOL__INFO__OPERATING__SET_

#define VUPRS_NETWORK_PROTOCOL__INFO__DATA__RAW_ALL_CHANNEL               (uint8_t)0x02
#define VUPRS_NETWORK_PROTOCOL__INFO__DATA__FFT_ALL_CHANNEL               (uint8_t)0x03
#define VUPRS_NETWORK_PROTOCOL__INFO__DATA__TARGET_SIGNAL                 (uint8_t)0x04
#define VUPRS_NEWWORK_PROTOCOL__INFO__DATA__SCANNING_AMPLITUDE            (uint8_t)0x05
#define VUPRS_NEWWORK_PROTOCOL__INFO__DATA__SCANNING_PROBABILITY          (uint8_t)0x05

#define IS_VUPRS_NEWWORK_PROTOCOL__INFO(VAL) \
(VAL == VUPRS_NETWORK_PROTOCOL__INFO__OPERATING_STATUS__OPERATING_SUCCESS || \
 VAL == VUPRS_NETWORK_PROTOCOL__INFO__OPERATING_STATUS__OPERATION_FAILED || \
 VAL == VUPRS_NETWORK_PROTOCOL__INFO__DATA__RAW_ALL_CHANNEL || \
 VAL == VUPRS_NETWORK_PROTOCOL__INFO__DATA__FFT_ALL_CHANNEL || \
 VAL == VUPRS_NETWORK_PROTOCOL__INFO__DATA__TARGET_SIGNAL || \
 VAL == VUPRS_NEWWORK_PROTOCOL__INFO__DATA__SCANNING_AMPLITUDE || \
 VAL == VUPRS_NEWWORK_PROTOCOL__INFO__DATA__SCANNING_PROBABILITY)

/**
 * @brief Status.
 */

#define VUPRS_NETWORK_PROTOCOL__STATUS__NORMAL                            (uint8_t)0x00
#define VUPRS_NETWORK_PROTOCOL__STATUS__ABNORMAL                          (uint8_t)0x01

#define IS_VUPRS_NETWORK_PROTOCOL__STATUS(VAL) \
(VAL == VUPRS_NETWORK_PROTOCOL__STATUS__NORMAL || \
 VAL == VUPRS_NETWORK_PROTOCOL__STATUS__ABNORMAL)

/**
 * @brief Position on header.
 */

#define __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__VERSION                  (uint32_t)2
#define __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__TYPE                     (uint32_t)3
#define __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__INFO                     (uint32_t)4
#define __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__STATUS                   (uint32_t)5

#if __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__VERSION > 7 || \
    __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__TYPE > 7 || \
    __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__INFO > 7 || \
    __VUPRS_NETWORK_PROTOCOL__BYTE_POSITION__STATUS > 7

#error "Position must smaller than 8."

#endif

/**
 * @brief Magic.
 */

#define __VUPRS_NETWORK_PROTOCOL__MAGIC__START_MAGIC                      (uint32_t)0xCAFEBABE
#define __VUPRS_NETWORK_PROTOCOL__MAGIC__END_MAGIC                        (uint32_t)0xCAFEBABF

#define __VUPRS_NETWORK_PROTOCOL__HEADER_SIZE                             (uint32_t)16  /* bytes (Magic and Data are not included) */

#define __VUPRS_NETWORK_PROTOCOL__SENDING_MAX_TRIES                       (uint32_t)5

namespace vuprs
{
    class NetworkProtocolMessage
    {
        private:

            uint8_t version, type, info, status;
            const void *data;
            uint32_t dataSize;

            std::vector<uint8_t> serializeData;

            bool versionSet = false, typeSet = false, infoSet = false, statusSet = false;
            bool set = false;

            void UpdateStatus();

            void Serialize();

        public:

            NetworkProtocolMessage();

            ~NetworkProtocolMessage();

            void clear();

            void SetVersion(uint8_t version);

            void SetType(uint8_t type);

            void SetInfo(uint8_t info);

            void SetStatus(uint8_t status);

            void SetData(const void *data, uint32_t size);

            void GenerateHeader(uint32_t *firstWord, uint32_t *secondWord) const;

            const uint8_t* GetData() const;
            uint32_t GetDataSize() const;

            bool SendToSocket(int client_fd, uint32_t timeout_ms = 5000);
    };

}

#endif