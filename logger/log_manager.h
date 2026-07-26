#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <sstream>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#define __LOGGER_FILE_MAX_SIZE__ 1024 * 1024 * 2 /* 2 MB */
#define __LOGGER_FILE_MAX_FILES__ 10             /* 10 files */

namespace vuprs
{
    class LogManager
    {
    private:
        static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;

    public:
        static std::shared_ptr<spdlog::logger> getLogger(const std::string &name, const std::string &filename = "");
        static bool loggerExists(const std::string &name) { return loggers_.find(name) != loggers_.end(); }
        static void clearLoggers() { loggers_.clear(); }
    };

    enum LogLevel
    {
        V_FATAL = 0,
        V_ERROR,
        V_WARN,
        V_INFO,
        V_DEBUG,
        V_TRACE
    };

    class LogStream
    {
    public:
        LogStream(std::shared_ptr<spdlog::logger> logger, LogLevel level)
            : logger_(logger), level_(level) {}

        ~LogStream()
        {
            if (logger_)
            {
                auto spd_level = mapLevel(level_);
                logger_->log(spd_level, ss_.str());
            }
        }

        template <typename T>
        LogStream &operator<<(const T &val)
        {
            ss_ << val;
            return *this;
        }

        LogStream &operator<<(std::ostream &(*manip)(std::ostream &))
        {
            ss_ << manip;
            return *this;
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        LogLevel level_;
        std::ostringstream ss_;

        static spdlog::level::level_enum mapLevel(LogLevel lvl)
        {
            switch (lvl)
            {
            case LogLevel::V_FATAL:
                return spdlog::level::critical;
            case LogLevel::V_ERROR:
                return spdlog::level::err;
            case LogLevel::V_WARN:
                return spdlog::level::warn;
            case LogLevel::V_INFO:
                return spdlog::level::info;
            case LogLevel::V_DEBUG:
                return spdlog::level::debug;
            case LogLevel::V_TRACE:
                return spdlog::level::trace;
            default:
                return spdlog::level::info;
            }
        }
    };
}

#define LOG(level, logger) LogStream(logger, level)
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