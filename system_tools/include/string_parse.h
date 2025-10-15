#ifndef STRING_PARSE_H
#define STRING_PARSE_H

#include <string>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace vuprs
{
    uint64_t ParseHexFromString(const std::string &dataString, bool *status);
    int ParseIntegerFromString(const std::string &dataString, bool *status);

    uint64_t ParseNumberFromString(const std::string &dataString, bool *status);
} 

#endif