#include "file_processing.h"

static std::string EnsureTrailingSlash(const std::string &path) 
{
    if (path.empty()) 
    {
        return "/";
    }
    char lastChar = path.back();
    if (lastChar != '/' && lastChar != '\\')
    {
        return path + "/";
    }
    return path;
}

std::string vuprs::FileAbsolutePath(const std::string &path, const std::string &fileName)
{
    #if __SOLVER_CXX_STANDARD__ >= 17
        std::filesystem::path dir(path), file(fileName);
        return (dir / file).string();
    #else
        std::string normalizedPath = EnsureTrailingSlash(path);
        return normalizedPath + fileName;
    #endif
}

std::string vuprs::AddPath(const std::string &basePath, const std::string &addedPath)
{
    #if __SOLVER_CXX_STANDARD__ >= 17
        std::filesystem::path path1(basePath), path2(addedPath);
        return (path1 / path2).string();
    #else
        std::string normalizedBase = EnsureTrailingSlash(basePath);
        return normalizedBase + addedPath;
    #endif
}

bool vuprs::PathExist(const std::string &path)
{
    #if __SOLVER_CXX_STANDARD__ >= 17
        return std::filesystem::exists(path);
    #else
        struct stat info;
        return stat(path.c_str(), &info) == 0;
    #endif
}

bool vuprs::FileExist(const std::string &file)
{
    return vuprs::PathExist(file);
}