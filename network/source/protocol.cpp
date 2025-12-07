#include "protocol.h"

vuprs::NetworkProtocolConfig vuprs::LoadNetworkProtocolConfigFromJson(const std::string &filename)
{
    if (filename.empty())
    {
        throw std::runtime_error("Invalid filename: " + filename);
    }
}

vuprs::NetworkProtocolConfig LoadNetworkProtocolConfigFromJson(const std::string &filename) 
{
    vuprs::NetworkProtocolConfig config;
        
    try 
    {  
        std::ifstream file(filename);
        if (!file.is_open()) 
        {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        nlohmann::json jsonData = nlohmann::json::parse(file);
        file.close();

        bool status = false;
            
        /* Basic */

        if (jsonData.contains("basic")) 
        {
            const auto& basic = jsonData["basic"];

            /* Magic */

            vuprs::__JsonStringParseINT<uint32_t>(&config.magic, basic, "magic", true);
                
            /* Response type */
                
            if (basic.contains("response_type")) 
            {
                const auto& responseType = basic["response_type"];
                    
                vuprs::__JsonStringParseINT<uint8_t>(&config.responseType.RSP__NONE, basic, "RSP__NONE", true);
                vuprs::__JsonStringParseINT<uint8_t>(&config.responseType.RSP__NORMAL, basic, "RSP__NORMAL", true);
                vuprs::__JsonStringParseINT<uint8_t>(&config.responseType.RSP__EMERGENCY, basic, "RSP__EMERGENCY", true);
            }
            else
            {
                throw std::runtime_error("Missing element: response_type");
            }
        }
        else
        {
            throw std::runtime_error("Missing element: basic");
        }
            
        /* Channels */

        if (jsonData.contains("channels"))
        {
            const auto& channels = jsonData["channels"];
            
            /* Control */

            if (channels.contains("control")) 
            {
                const auto& control = channels["control"];
                
                /* Channel ID */

                vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.ChannelID, control, "id", true);
                
                /* Message */

                if (control.contains("message")) 
                {
                    const auto& messages = control["message"];
                    
                    if (messages.contains("setting")) 
                    {
                        const auto& setting = messages["setting"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__START_SAMPLING, setting, "CONTROL__MSG__START_SAMPLING", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__STOP_SAMPLING, setting, "CONTROL__MSG__STOP_SAMPLING", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__DISCONNECT, setting, "CONTROL__MSG__DISCONNECT", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__RESET, setting, "CONTROL__MSG__RESET", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__SET_SAMPLING_FREQUENCY, setting, "CONTROL__MSG__SET_SAMPLING_FREQUENCY", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__SET_DATA_PACKAGE_SIZE, setting, "CONTROL__MSG__SET_DATA_PACKAGE_SIZE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.settingMessage.CONTROL__MSG__SET_DATA_TYPE, setting, "CONTROL__MSG__SET_DATA_TYPE", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: setting");
                    }
                    if (messages.contains("query"))
                    {
                        const auto& query = messages["query"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.queryMessage.CONTROL__MSG__QUERY_SAMPLING_FREQUENCY, query, "CONTROL__MSG__QUERY_SAMPLING_FREQUENCY", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.queryMessage.CONTROL__MSG__QUERY_DATA_TYPE, query, "CONTROL__MSG__QUERY_DATA_TYPE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.queryMessage.CONTROL__MSG__QUERY_DEVICE_ID, query, "CONTROL__MSG__QUERY_DEVICE_ID", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: query");
                    }
                    if (messages.contains("response")) 
                    {
                        const auto& response = messages["response"];
                        
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__SUCCESS, response, "CONTROL__MSG__SUCCESS", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__FAILED, response, "CONTROL__MSG__FAILED", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__DISCONNECT_READY, response, "CONTROL__MSG__DISCONNECT_READY", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__DEVICE_ID, response, "CONTROL__MSG__DEVICE_ID", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__SAMPLING_FREQUENCY, response, "CONTROL__MSG__SAMPLING_FREQUENCY", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.controlChannelConfig.responseMessage.CONTROL__MSG__DATA_TYPE, response, "CONTROL__MSG__DATA_TYPE", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: response");
                    }
                }
                else
                {
                    throw std::runtime_error("Missing element: message");
                }
            }
            else
            {
                throw std::runtime_error("Missing element: control");
            }
            
            /* Notify */

            if (channels.contains("notify"))
            {
                const auto& notify = channels["notify"];
                
                vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.ChannelID, notify, "id", true);

                if (notify.contains("message")) 
                {
                    const auto& messages = notify["message"];
                    
                    if (messages.contains("query")) 
                    {
                        const auto& query = messages["query"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.queryMessage.NOTIFY__MSG__QUERY_CPU_TEMPERATURE, query, "NOTIFY__MSG__QUERY_CPU_TEMPERATURE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.queryMessage.NOTIFY__MSG__QUERY_STORAGE, query, "NOTIFY__MSG__QUERY_STORAGE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.queryMessage.NOTIFY__MSG__QUERY_BATTERY_LEVEL, query, "NOTIFY__MSG__QUERY_BATTERY_LEVEL", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: query");
                    }
                    if (messages.contains("response")) 
                    {
                        const auto& response = messages["response"];
                        
                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.responseMessage.NOTIFY__MSG__CPU_TEMPERATURE, response, "NOTIFY__MSG__CPU_TEMPERATURE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.responseMessage.NOTIFY__MSG__STORAGE, response, "NOTIFY__MSG__STORAGE", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.notifyChannelConfig.responseMessage.NOTIFY__MSG__BATTERY_LEVEL, response, "NOTIFY__MSG__BATTERY_LEVEL", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: response");
                    }
                }
                else
                {
                    throw std::runtime_error("Missing element: message");
                }
            }
            else
            {
                throw std::runtime_error("Missing element: notify");
            }
            
            /* Heartbeat */

            if (channels.contains("heartbeat")) 
            {
                const auto& heartbeat = channels["heartbeat"];
                
                vuprs::__JsonStringParseINT<uint8_t>(&config.heartbeatChannelConfig.ChannelID, heartbeat, "id", true);
                
                if (heartbeat.contains("message")) 
                {
                    const auto& messages = heartbeat["message"];
                    
                    if (messages.contains("emergency")) 
                    {
                        const auto& emergency = messages["emergency"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.heartbeatChannelConfig.emergencyMessage.HEARTBEAT__MSG__EMERGENCY_ACK, emergency, "HEARTBEAT__MSG__EMERGENCY_ACK", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: emergency");
                    }
                    if (messages.contains("response")) 
                    {
                        const auto& response = messages["response"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.heartbeatChannelConfig.responseMessage.HEARTBEAT__MSG__IS_ONLINE, response, "HEARTBEAT__MSG__IS_ONLINE", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: response");
                    }
                }
                else
                {
                    throw std::runtime_error("Missing element: message");
                }
            }
            else
            {
                throw std::runtime_error("Missing element: heartbeat");
            }

            /* Data */

            if (channels.contains("data"))
            {
                const auto& data = channels["data"];

                vuprs::__JsonStringParseINT<uint8_t>(&config.dataChannelConfig.ChannelID, data, "id", true);
                
                if (data.contains("message")) 
                {
                    const auto& messages = data["message"];

                    if (messages.contains("data")) 
                    {
                        const auto& dataMsgs = messages["data"];

                        vuprs::__JsonStringParseINT<uint8_t>(&config.dataChannelConfig.dataMessage.DATA__MSG__RAW_SIGNAL, dataMsgs, "DATA__MSG__RAW_SIGNAL", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.dataChannelConfig.dataMessage.DATA__MSG__TARGET_SIGNAL, dataMsgs, "DATA__MSG__TARGET_SIGNAL", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.dataChannelConfig.dataMessage.DATA__MSG__SCAN_POWER, dataMsgs, "DATA__MSG__SCAN_POWER", true);
                        vuprs::__JsonStringParseINT<uint8_t>(&config.dataChannelConfig.dataMessage.DATA__MSG__SCAN_PROBABILITY__FAULT_1, dataMsgs, "DATA__MSG__SCAN_PROBABILITY__FAULT_1", true);
                    }
                    else
                    {
                        throw std::runtime_error("Missing element: data");
                    }
                }
                else
                {
                    throw std::runtime_error("Missing element: message");
                }
            }
            else
            {
                throw std::runtime_error("Missing element: data");
            }
        }
        else
        {
            throw std::runtime_error("Missing element: channels");
        }  
    } 
    catch (const nlohmann::json::parse_error& e) 
    {
        throw std::runtime_error("Error occurred in parsing: (" + std::string(e.what()) + ").");
    } 
    catch (const std::exception& e)
    {
        throw std::runtime_error("Error occurred in parsing: (" + std::string(e.what()) + ").");
    }
    
    return config;
}

void vuprs::SetDefaultHeaderValue(vuprs::NetworkProtocolHeader *header)
{
    if (header != nullptr) memset(header, 0, sizeof(*header));
    else throw std::runtime_error("Header cannot be NULL.");
}

vuprs::NetworkProtocolHeader vuprs::Buffer2NetworkHeader(const char *buffer, size_t bufferSize, uint32_t magic)
{
    if (!buffer) 
    {
        throw std::invalid_argument("Buffer2NetworkHeader: null buffer pointer");
    }
    
    constexpr size_t HEADER_SIZE = sizeof(vuprs::NetworkProtocolHeader);
    static_assert(HEADER_SIZE == VUPRS_NETWORK_PROTOCOL_HEADER_BYTE_SIZE, "Header size must be 44 bytes");
    
    if (bufferSize < HEADER_SIZE)
    {
        throw std::length_error("Buffer2NetworkHeader: buffer too small (" + std::to_string(bufferSize) + " < " + std::to_string(HEADER_SIZE) + ")");
    }
    
    vuprs::NetworkProtocolHeader header;
    std::memcpy(&header, buffer, HEADER_SIZE);
    
    constexpr uint32_t PROTOCOL_MAGIC = magic;
    
    if (header.magic != PROTOCOL_MAGIC)
    {
        uint32_t network_magic = ntohl(header.magic);
        if (network_magic == PROTOCOL_MAGIC) 
        {
            vuprs::ConvertHeaderToHostByteOrder(&header);
        }
        else
        {
            throw std::runtime_error("Invalid protocol magic: " + vuprs::Number2HexString(network_magic));
        }
    }
    
    return header;
}

void vuprs::ConvertHeaderToHostByteOrder(NetworkProtocolHeader* header)
{
    if (!header) throw std::runtime_error("Cannot convert NULL header pointer.");
    
    header->magic = ntohl(header->magic);
    header->msg_id = ntohl(header->msg_id);
    header->msg_response_id = ntohl(header->msg_response_id);
    header->body_size = ntohl(header->body_size);
    header->udp_stream_id = ntohl(header->udp_stream_id);
    header->udp_packet_sequence = ntohl(header->udp_packet_sequence);
    header->udp_total_packet_count = ntohl(header->udp_total_packet_count);
    header->udp_total_packet_size = ntohl(header->udp_total_packet_size);
}
