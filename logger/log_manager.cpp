#include "config.h"
#include "logger/log_manager.h"
#include "system_tools/file_processing.h"
#if VUPRS_HAS_FILESYSTEM
#include <filesystem>
#endif

std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> vuprs::LogManager::loggers_;

std::shared_ptr<spdlog::logger> vuprs::LogManager::getLogger(const std::string &name,
                                                             const std::string &filename,
                                                             const std::string &dir_name)
{
    auto it = loggers_.find(name);
    /* Return an exist logger */
    if (it != loggers_.end())
    {
        return it->second;
    }
    /* Create a new logger */
    std::string _dir = dir_name.empty() ? "./logs" : dir_name;
    if (!vuprs::PathExist(_dir))
    {
#if VUPRS_HAS_FILESYSTEM
        std::filesystem::create_directories(_dir);
#else
#ifdef _WIN32
        _mkdir(_dir.c_str());
#else
        mkdir(_dir.c_str(), 0777);
#endif
#endif
    }
    std::shared_ptr<spdlog::logger> logger;
    if (filename.empty())
    {
        logger = spdlog::stdout_color_mt(name);
    }
    else
    {
        logger = spdlog::rotating_logger_mt(name,
                                            vuprs::FileAbsolutePath(_dir, filename),
                                            __LOGGER_FILE_MAX_SIZE__,
                                            __LOGGER_FILE_MAX_FILES__);
    }
#if DEBUG
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::debug);
#else
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
#endif
    logger->set_pattern("[%Y-%-m-%-dT%H:%M:%S.%e%z] [%l] %v");
    loggers_[name] = logger;
    return logger;
}
