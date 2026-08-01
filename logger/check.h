#ifndef V_CHECK_H
#define V_CHECK_H

#include <string>
#include <cstdlib>
#include <cstring>

#define _CHECK(header, condition, api, info)                                              \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            const char *_v_file = __FILE__;                                               \
            const char *_v_filename = std::strrchr(_v_file, '/');                         \
            if (!_v_filename)                                                             \
                _v_filename = std::strrchr(_v_file, '\\');                                \
            if (_v_filename)                                                              \
                ++_v_filename;                                                            \
            else                                                                          \
                _v_filename = _v_file;                                                    \
            throw std::runtime_error("[" + std::string(api) + "] " +                      \
                                     std::string(header) + " in [" +                      \
                                     std::string(_v_filename) + "] (line " +              \
                                     std::to_string(__LINE__) + ")" + std::string(info)); \
        }                                                                                 \
    } while (0)

#define RUNTIME_CHECK(condition, api, info) _CHECK("Runtime check failed", condition, api, info)
#define PARAM_CHECK(condition, api, info) _CHECK("Param check failed", condition, api, info)

#endif
