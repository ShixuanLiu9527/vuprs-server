#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <sstream>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "config.h"
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
        static std::shared_ptr<spdlog::logger> getLogger(const std::string &name,
                                                         const std::string &filename = "",
                                                         const std::string &dir_name = "");
        static bool loggerExists(const std::string &name) { return loggers_.find(name) != loggers_.end(); }
        static void clearLoggers() { loggers_.clear(); }
    };

    enum LogLevel
    {
        _V_FATAL = 0,
        _V_ERROR,
        _V_WARN,
        _V_INFO,
        _V_DEBUG,
        _V_TRACE
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
            case LogLevel::_V_FATAL:
                return spdlog::level::critical;
            case LogLevel::_V_ERROR:
                return spdlog::level::err;
            case LogLevel::_V_WARN:
                return spdlog::level::warn;
            case LogLevel::_V_INFO:
                return spdlog::level::info;
            case LogLevel::_V_DEBUG:
                return spdlog::level::debug;
            case LogLevel::_V_TRACE:
                return spdlog::level::trace;
            default:
                return spdlog::level::info;
            }
        }
    };
}

#define V_FATAL vuprs::LogLevel::_V_FATAL /* Log level: fatal */
#define V_ERROR vuprs::LogLevel::_V_ERROR /* Log level: error */
#define V_WARN vuprs::LogLevel::_V_WARN   /* Log level: warn */
#define V_INFO vuprs::LogLevel::_V_INFO   /* Log level: info */
#define V_DEBUG vuprs::LogLevel::_V_DEBUG /* Log level: debug */
#define V_TRACE vuprs::LogLevel::_V_TRACE /* Log level: trace */
#if DEBUG
#define _LOG(logger, level) vuprs::LogStream(logger, level)
#define _LOG_BY_NAME(logger, level) vuprs::LogStream(vuprs::LogManager::getLogger(logger), level)
#else /* DEBUG = false: compile out V_DEBUG / V_TRACE level logs entirely */
#define _LOG(logger, level)                 \
    if ((level) > vuprs::LogLevel::_V_INFO) \
    {                                       \
    }                                       \
    else                                    \
        vuprs::LogStream(logger, level)
#define _LOG_BY_NAME(logger, level)         \
    if ((level) > vuprs::LogLevel::_V_INFO) \
    {                                       \
    }                                       \
    else                                    \
        vuprs::LogStream(vuprs::LogManager::getLogger(logger), level)
#endif

#endif