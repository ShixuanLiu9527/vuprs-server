#include "system_tools/string_parse.h"
#include "logger/log_manager.h"

uint64_t vuprs::ParseHexFromString(const std::string &dataString, bool *status)
{
    std::string hexString = dataString;
    if (status != nullptr)
    {
        *status = false;
    }
    if (dataString.empty() || (dataString.substr(0, 2) != "0X" && dataString.substr(0, 2) != "0x"))
    {
        return 0;
    }
    std::transform(hexString.begin(), hexString.end(), hexString.begin(), ::toupper);
    try
    {
        hexString.erase(std::remove(hexString.begin(), hexString.end(), '_'), hexString.end());

        if (hexString.length() >= 3)
        {
            /* Check Digital Value */
            for (size_t i = 2; i < hexString.length(); i++)
            {
                if (!std::isxdigit(hexString[i]))
                {
                    return 0;
                }
            }
            if (status != nullptr)
            {
                *status = true;
            }
            return (uint64_t)(std::stoull(hexString, nullptr, 16));
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

int vuprs::ParseIntegerFromString(const std::string &dataString, bool *status)
{
    std::string parseString = dataString;
    if (status != nullptr)
    {
        (*status) = false;
    }
    if (dataString.empty())
    {
        return 0;
    }
    try
    {
        parseString.erase(std::remove(parseString.begin(), parseString.end(), '_'), parseString.end());
        /* Check Digital Value */
        for (size_t i = 0; i < parseString.length(); i++)
        {
            if (!std::isdigit(parseString[i]))
            {
                return 0;
            }
        }
        if (status != nullptr)
        {
            (*status) = true;
        }
        return std::stoi(parseString);
    }
    catch (const std::exception &e)
    {
        return 0;
    }
}

double vuprs::ParseDoubleFromString(const std::string &dataString, bool *status)
{
    if (status != nullptr)
    {
        (*status) = false;
    }
    if (dataString.empty())
    {
        return 0.0;
    }
    try
    {
        double retValue = std::stod(dataString);
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

uint64_t vuprs::ParseNumberFromString(const std::string &dataString, bool *status)
{
    bool parseStatus = false;
    uint64_t parseData;
    if (status != nullptr)
        (*status) = false;
    parseData = vuprs::ParseHexFromString(dataString, &parseStatus);
    if (parseStatus)
    {
        if (status != nullptr)
            (*status) = true;
        return parseData;
    }
    parseData = vuprs::ParseIntegerFromString(dataString, &parseStatus);
    if (parseStatus)
    {
        if (status != nullptr)
            (*status) = true;
        return parseData;
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
    const bool hasHeader = (!header.empty() && result.size() >= header.size() &&
                            result.compare(0, header.size(), header) == 0);
    const bool hasTailer = (!tailer.empty() && result.size() >= tailer.size() &&
                            result.compare(result.size() - tailer.size(), tailer.size(), tailer) == 0);
    if (hasHeader && hasTailer)
    {
        return result;
    }
    if (!hasHeader && !header.empty())
    {
        result = header + result;
    }
    if (!hasTailer && !tailer.empty())
    {
        result += tailer;
    }
    return result;
}
