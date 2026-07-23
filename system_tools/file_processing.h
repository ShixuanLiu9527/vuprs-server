#ifndef FILE_PROCESSING_H
#define FILE_PROCESSING_H

#include <string>
#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

namespace vuprs
{
    std::string FileAbsolutePath(const std::string &path, const std::string &fileName);
    std::string AddPath(const std::string &basePath, const std::string &addedPath);
    void SplitFile(const std::string &fullpath, std::string *dir, std::string *filename, std::string *extension);
    bool PathExist(const std::string &path);
    bool FileExist(const std::string &file);
    bool MakeDir(const std::string &dir);
}

#endif