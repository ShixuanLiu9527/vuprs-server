#ifndef STRING_PARSE_H
#define STRING_PARSE_H

#include <string>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include "3rdparty/nlohmann/json.hpp"
#include "logger/log_manager.h"

namespace vuprs
{
    uint64_t ParseHexFromString(const std::string &dataString, bool *status);
    int ParseIntegerFromString(const std::string &dataString, bool *status);
    double ParseDoubleFromString(const std::string &dataString, bool *status);
    uint64_t ParseNumberFromString(const std::string &dataString, bool *status);
    std::string Number2HexString(const uint64_t &num);

    template <typename T>
    int FindValueInVec(const std::vector<T> &vec, const T &value)
    {
        auto it = std::find(vec.begin(), vec.end(), value);
        if (it != vec.end())
        {
            return std::distance(vec.begin(), it);
        }
        return -1;
    }

    template <typename T>
    void __JsonStringParseINT(T *target, const nlohmann::json &json, const std::string &item, bool required = true)
    {
        PARAM_CHECK(target != nullptr, "system_tools", " in [__JsonStringParseINT] Target cannot be NULL.");
        bool status = false;
        uint64_t value;
        if (json.contains(item))
        {
            value = vuprs::ParseNumberFromString(json[item].get<std::string>(), &status);
            if (status)
                *target = static_cast<T>(value);
            else if (required)
                RUNTIME_CHECK(false, "system_tools", " in [__JsonStringParseINT] Cannot parse: " + item + " from json.");
        }
        else if (required)
        {
            PARAM_CHECK(false, "system_tools", " in [__JsonStringParseINT] Item: " + item + " not found.");
        }
    }

    template <typename T>
    void __JsonStringParseFLOAT(T *target, const nlohmann::json &json, const std::string &item, bool required = true)
    {
        PARAM_CHECK(target != nullptr, "system_tools", " in [__JsonStringParseFLOAT] Target cannot be NULL.");
        bool status = false;
        double value;
        if (json.contains(item))
        {
            value = vuprs::ParseDoubleFromString(json[item].get<std::string>(), &status);
            if (status)
                *target = static_cast<T>(value);
            else if (required)
                RUNTIME_CHECK(false, "system_tools", " in [__JsonStringParseFLOAT] Cannot parse: " + item + " from json.");
        }
        else if (required)
        {
            PARAM_CHECK(false, "system_tools", " in [__JsonStringParseFLOAT] Item: " + item + " not found.");
        }
    }

    void __JsonParseString(std::string *target, const nlohmann::json &json, const std::string &item, bool required = true);

    /**
     * @brief Remove frame header/tailer if exists on boundaries.
     *
     * @note If header exists at message beginning, remove it.
     * @note If tailer exists at message ending, remove it.
     * @note If not found, keep message unchanged.
     */
    std::string RemoveFrameIfExists(const std::string &message, const std::string &header, const std::string &tailer);

    /**
     * @brief Ensure frame header/tailer exist on boundaries.
     *
     * @note If both header and tailer already exist, keep unchanged.
     * @note Otherwise add missing part(s).
     */
    std::string AddFrameIfMissing(const std::string &message, const std::string &header, const std::string &tailer);
}

#endif
