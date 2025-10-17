#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#define __LOGGER_FILE_MAX_SIZE__        1024 * 1024 * 1   /* 1 MB */
#define __LOGGER_FILE_MAX_FILES__       10  /* 10 files */

namespace vuprs
{
    class LogManager
    {
        private:

            static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;

        public:

            static std::shared_ptr<spdlog::logger> getLogger(const std::string& name, const std::string& filename = "");

            static bool loggerExists(const std::string& name) {return loggers_.find(name) != loggers_.end();}

            static void clearLoggers() {loggers_.clear();}
    };
}

#endif