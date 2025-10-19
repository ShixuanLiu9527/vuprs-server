#ifndef FILE_PROCESSING_H
#define FILE_PROCESSING_H

#include <string>
#include <sys/stat.h>

namespace vuprs
{
    std::string FileAbsolutePath(const std::string &path, const std::string &fileName);
    std::string AddPath(const std::string &basePath, const std::string &addedPath);
    bool PathExist(const std::string &path);
    bool FileExist(const std::string &file);
}

#endif