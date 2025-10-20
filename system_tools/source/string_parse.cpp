#include "string_parse.h"

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

uint64_t vuprs::ParseNumberFromString(const std::string &dataString, bool *status)
{
    bool parseStatus = false;
    uint64_t parseData;

    if (status != nullptr) (*status) = false;

    parseData = vuprs::ParseHexFromString(dataString, &parseStatus);

    if (parseStatus)
    {
        if (status != nullptr) (*status) = true;
        return parseData;
    }

    parseData = vuprs::ParseIntegerFromString(dataString, &parseStatus);

    if (parseStatus)
    {
        if (status != nullptr) (*status) = true;
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
