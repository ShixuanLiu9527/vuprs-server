#include "log_manager.h"

std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> vuprs::LogManager::loggers_;

std::shared_ptr<spdlog::logger> vuprs::LogManager::getLogger(const std::string& name, const std::string& filename)
{
    auto it = loggers_.find(name);

    /* Return an exist logger */

    if (it != loggers_.end()) 
    {
        return it->second;
    }

    /* Create a new logger */

    std::shared_ptr<spdlog::logger> logger;
    
    if (filename.empty()) 
    {
        logger = spdlog::stdout_color_mt(name);
    } 
    else 
    {
        logger = spdlog::rotating_logger_mt(name, filename, __LOGGER_FILE_MAX_SIZE__, __LOGGER_FILE_MAX_FILES__);
    }

    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    logger->set_pattern("[%Y-%-m-%-dT%H:%M:%S.%e%z] [%l] %v");

    loggers_[name] = logger;

    return logger;
}
