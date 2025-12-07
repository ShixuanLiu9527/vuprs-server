#ifndef VUPRS_PROTOCOL_H
#define VUPRS_PROTOCOL_H

#include <stdint.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "nlohmann/json.hpp"
#include "string_parse.h"

namespace vuprs
{
    #define VUPRS_NETWORK_PROTOCOL_HEADER_BYTE_SIZE 36U

    #pragma pack(push, 1)
    struct NetworkProtocolHeader
    {
        uint32_t magic;

        uint8_t version;
        uint8_t channel;
        uint8_t msg_type;
        uint8_t response_flag;

        uint32_t msg_id;
        uint32_t msg_response_id;

        uint32_t body_size;

        uint32_t udp_stream_id;
        uint32_t udp_packet_sequence;
        uint32_t udp_total_packet_count;
        uint32_t udp_total_packet_size;
    };
    #pragma pack(pop)

    /* ----------------------------------------------------------------------------------------------------- */
    /* ----------------------------------------- Config Channel -------------------------------------------- */
    /* ----------------------------------------------------------------------------------------------------- */

    struct ControlChannel_Message_Setting
    {
        uint8_t CONTROL__MSG__START_SAMPLING;
        uint8_t CONTROL__MSG__STOP_SAMPLING;
        uint8_t CONTROL__MSG__DISCONNECT;
        uint8_t CONTROL__MSG__RESET;
        uint8_t CONTROL__MSG__SET_SAMPLING_FREQUENCY;
        uint8_t CONTROL__MSG__SET_DATA_PACKAGE_SIZE;
        uint8_t CONTROL__MSG__SET_DATA_TYPE;
    };
    
    struct ControlChannel_Message_Query
    {
        uint8_t CONTROL__MSG__QUERY_SAMPLING_FREQUENCY;
        uint8_t CONTROL__MSG__QUERY_DATA_TYPE;
        uint8_t CONTROL__MSG__QUERY_DEVICE_ID;
    };
    
    struct ControlChannel_Message_Response
    {
        uint8_t CONTROL__MSG__SUCCESS;
        uint8_t CONTROL__MSG__FAILED;
        uint8_t CONTROL__MSG__DISCONNECT_READY;
        uint8_t CONTROL__MSG__DEVICE_ID;
        uint8_t CONTROL__MSG__SAMPLING_FREQUENCY;
        uint8_t CONTROL__MSG__DATA_TYPE;
    };
    
    struct ControlChannel_Config
    {
        uint8_t ChannelID;

        vuprs::ControlChannel_Message_Setting settingMessage;
        vuprs::ControlChannel_Message_Query queryMessage;
        vuprs::ControlChannel_Message_Response responseMessage;
    };

    /* ----------------------------------------------------------------------------------------------------- */
    /* ----------------------------------------- Notify Channel -------------------------------------------- */
    /* ----------------------------------------------------------------------------------------------------- */
    
    struct NotifyChannel_Message_Query
    {
        uint8_t NOTIFY__MSG__QUERY_CPU_TEMPERATURE;
        uint8_t NOTIFY__MSG__QUERY_STORAGE;
        uint8_t NOTIFY__MSG__QUERY_BATTERY_LEVEL;
    };
    
    struct NotifyChannel_Message_Response
    {
        uint8_t NOTIFY__MSG__CPU_TEMPERATURE;
        uint8_t NOTIFY__MSG__STORAGE;
        uint8_t NOTIFY__MSG__BATTERY_LEVEL;
    };
    
    struct NotifyChannel_Config
    {
        uint8_t ChannelID;

        vuprs::NotifyChannel_Message_Query queryMessage;
        vuprs::NotifyChannel_Message_Response responseMessage;
    };

    /* ----------------------------------------------------------------------------------------------------- */
    /* ----------------------------------------- Heartbeat Channel ----------------------------------------- */
    /* ----------------------------------------------------------------------------------------------------- */

    struct HeartbeatChannel_Message_Emergency
    {
        uint8_t HEARTBEAT__MSG__EMERGENCY_ACK;
    };
    
    struct HeartbeatChannel_Message_Response
    {
        uint8_t HEARTBEAT__MSG__IS_ONLINE;
    };
    
    struct HeartbeatChannel_Config
    {
        uint8_t ChannelID;

        vuprs::HeartbeatChannel_Message_Emergency emergencyMessage;
        vuprs::HeartbeatChannel_Message_Response responseMessage;
    };

    /* ----------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------- Data Channel ------------------------------------------- */
    /* ----------------------------------------------------------------------------------------------------- */

    struct DataChannel_Message_Data
    {
        uint8_t DATA__MSG__RAW_SIGNAL;
        uint8_t DATA__MSG__TARGET_SIGNAL;
        uint8_t DATA__MSG__SCAN_POWER;
        uint8_t DATA__MSG__SCAN_PROBABILITY__FAULT_1;
    };
    
    struct DataChannel_Config
    {
        uint8_t ChannelID;

        vuprs::DataChannel_Message_Data dataMessage;
    };

    /* ----------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------- Total config ------------------------------------------- */
    /* ----------------------------------------------------------------------------------------------------- */

    struct NetworkProtocol_ResponseType
    {
        uint8_t RSP__NONE;
        uint8_t RSP__NORMAL;
        uint8_t RSP__EMERGENCY;
    };

    struct NetworkProtocolConfig
    {
        uint32_t magic;

        NetworkProtocol_ResponseType responseType;

        ControlChannel_Config controlChannelConfig;
        NotifyChannel_Config notifyChannelConfig;
        HeartbeatChannel_Config heartbeatChannelConfig;
        DataChannel_Config dataChannelConfig;
    };

    /**
     * @brief Set network protocol header value to default.
     * 
     * @param header target header.
     */
    void SetDefaultHeaderValue(vuprs::NetworkProtocolHeader *header);

    /**
     * @brief Load network protocol config from JSON file.
     * 
     * @param filename JSON filename.
     */
    vuprs::NetworkProtocolConfig LoadNetworkProtocolConfigFromJson(const std::string &filename);

    vuprs::NetworkProtocolHeader Buffer2NetworkHeader(const char *buffer, size_t bufferSize, uint32_t magic);

    void ConvertHeaderToHostByteOrder(NetworkProtocolHeader* header);
}

#endif