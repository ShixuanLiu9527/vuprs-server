#include "system_tools/string_parse.h"
#include "logger/log_manager.h"

uint64_t vuprs::ParseHexFromString(const std::string &data_string, bool *status)
{
    std::string hex_string = data_string;
    if (status != nullptr)
    {
        *status = false;
    }
    if (data_string.empty() || (data_string.substr(0, 2) != "0X" && data_string.substr(0, 2) != "0x"))
    {
        return 0;
    }
    std::transform(hex_string.begin(), hex_string.end(), hex_string.begin(), ::toupper);
    try
    {
        hex_string.erase(std::remove(hex_string.begin(), hex_string.end(), '_'), hex_string.end());
        if (hex_string.length() >= 3)
        {
            /* Check Digital Value */
            for (size_t i = 2; i < hex_string.length(); i++)
            {
                if (!std::isxdigit(hex_string[i]))
                {
                    return 0;
                }
            }
            if (status != nullptr)
            {
                *status = true;
            }
            return (uint64_t)(std::stoull(hex_string, nullptr, 16));
        }
        else
        {
            return 0;
        }
    }
    catch (...)
    {
        return 0;
    }
}

int vuprs::ParseIntegerFromString(const std::string &data_string, bool *status)
{
    std::string parse_string = data_string;
    if (status != nullptr)
    {
        (*status) = false;
    }
    if (data_string.empty())
    {
        return 0;
    }
    try
    {
        parse_string.erase(std::remove(parse_string.begin(), parse_string.end(), '_'), parse_string.end());
        /* Check Digital Value */
        for (size_t i = 0; i < parse_string.length(); i++)
        {
            if (!std::isdigit(parse_string[i]))
            {
                return 0;
            }
        }
        if (status != nullptr)
        {
            (*status) = true;
        }
        return std::stoi(parse_string);
    }
    catch (const std::exception &e)
    {
        return 0;
    }
}

double vuprs::ParseDoubleFromString(const std::string &data_string, bool *status)
{
    if (status != nullptr)
    {
        (*status) = false;
    }
    if (data_string.empty())
    {
        return 0.0;
    }
    try
    {
        double retValue = std::stod(data_string);
        if (status != nullptr)
        {
            (*status) = true;
        }
        return retValue;
    }
    catch (const std::exception &e)
    {
        return 0.0;
    }
}

uint64_t vuprs::ParseNumberFromString(const std::string &data_string, bool *status)
{
    bool parse_status = false;
    uint64_t parse_data;
    if (status != nullptr)
        (*status) = false;
    parse_data = vuprs::ParseHexFromString(data_string, &parse_status);
    if (parse_status)
    {
        if (status != nullptr)
            (*status) = true;
        return parse_data;
    }
    parse_data = vuprs::ParseIntegerFromString(data_string, &parse_status);
    if (parse_status)
    {
        if (status != nullptr)
            (*status) = true;
        return parse_data;
    }
    return 0;
}

std::string vuprs::Number2HexString(const uint64_t &num)
{
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase << num;
    return ss.str();
}

void vuprs::__JsonParseString(std::string *target, const nlohmann::json &json, const std::string &item, bool required)
{
    PARAM_CHECK(target != nullptr, "system_tools", " in [__JsonParseString] Target cannot be NULL.");
    if (json.contains(item))
    {
        *target = json[item].get<std::string>();
    }
    else if (required)
    {
        PARAM_CHECK(false, "system_tools", " in [__JsonParseString] Item: " + item + " not found.");
    }
}

std::string vuprs::RemoveFrameIfExists(const std::string &message, const std::string &header, const std::string &tailer)
{
    std::string result = message;
    if (!header.empty() && result.size() >= header.size() &&
        result.compare(0, header.size(), header) == 0)
    {
        result.erase(0, header.size());
    }
    if (!tailer.empty() && result.size() >= tailer.size() &&
        result.compare(result.size() - tailer.size(), tailer.size(), tailer) == 0)
    {
        result.erase(result.size() - tailer.size(), tailer.size());
    }
    return result;
}

std::string vuprs::AddFrameIfMissing(const std::string &message, const std::string &header, const std::string &tailer)
{
    std::string result = message;
    const bool has_header = (!header.empty() && result.size() >= header.size() &&
                             result.compare(0, header.size(), header) == 0);
    const bool has_tailer = (!tailer.empty() && result.size() >= tailer.size() &&
                             result.compare(result.size() - tailer.size(), tailer.size(), tailer) == 0);
    if (has_header && has_tailer)
    {
        return result;
    }
    if (!has_header && !header.empty())
    {
        result = header + result;
    }
    if (!has_tailer && !tailer.empty())
    {
        result += tailer;
    }
    return result;
}
