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

namespace vuprs
{
    uint64_t ParseHexFromString(const std::string &dataString, bool *status);
    int ParseIntegerFromString(const std::string &dataString, bool *status);
    double ParseDoubleFromString(const std::string &dataString, bool *status);
    uint64_t ParseNumberFromString(const std::string &dataString, bool *status);
    std::string Number2HexString(const uint64_t &num);

    template<typename T>
    int FindValueInVec(const std::vector<T> &vec, const T &value)
    {
        auto it = std::find(vec.begin(), vec.end(), value);
        if (it != vec.end()) 
        {
            return std::distance(vec.begin(), it);
        }
        return -1;
    }

    template<typename T>
    void __JsonStringParseINT(T *target, const nlohmann::json &json, const std::string &item, bool required = true)
    {
        if (target == nullptr) throw std::runtime_error("Target cannot be NULL.");
        bool status = false;
        uint64_t value;
        if (json.contains(item))
        {
            value = vuprs::ParseNumberFromString(json[item].get<std::string>(), &status);
            if (status) *target = static_cast<T>(value);
            else if (required) throw std::runtime_error("Cannot parse: " + item + " from json.");
        }
        else if (required)
        {
            throw std::runtime_error("Item: " + item + " not found.");
        }
    }

    template<typename T>
    void __JsonStringParseFLOAT(T *target, const nlohmann::json &json, const std::string &item, bool required = true)
    {
        if (target == nullptr) throw std::runtime_error("Target cannot be NULL.");
        bool status = false;
        uint64_t value;
        if (json.contains(item))
        {
            value = vuprs::ParseDoubleFromString(json[item].get<std::string>(), &status);
            if (status) *target = static_cast<T>(value);
            else if (required) throw std::runtime_error("Cannot parse: " + item + " from json.");
        }
        else if (required)
        {
            throw std::runtime_error("Item: " + item + " not found.");
        }
    }

    void __JsonParseString(std::string *target, const nlohmann::json &json, const std::string &item, bool required = true)
    {
        if (target == nullptr) throw std::runtime_error("Target cannot be NULL.");
        if (json.contains(item))
        {
            *target = json[item].get<std::string>();
        }
        else if (required)
        {
            throw std::runtime_error("Item: " + item + " not found.");
        }
    }
} 

#endif