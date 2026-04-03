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

void vuprs::SplitFile(const std::string &fullpath, std::string *dir, std::string *filename, std::string *extension)
{
    size_t slash_pos = fullpath.find_last_of("/\\");
    size_t dot_pos = fullpath.find_last_of('.');
    
    if (dir != nullptr)
    {
        if (slash_pos == std::string::npos) *dir = "";  /* directory not found */
        else *dir = fullpath.substr(0, slash_pos + 1);  /* string before '/' */
    }
    if (filename != nullptr)
    {
        size_t name_start = (slash_pos == std::string::npos) ? 0 : slash_pos + 1;
        size_t name_end = fullpath.length();  /* string after '/' */
        
        if (dot_pos != std::string::npos && dot_pos > slash_pos) name_end = dot_pos;
            
        *filename = fullpath.substr(name_start, name_end - name_start);
    }
    if (extension != nullptr)
    {
        if (dot_pos != std::string::npos && dot_pos > slash_pos) *extension = fullpath.substr(dot_pos + 1);
        else *extension = "";
    }
}

std::string vuprs::FileAbsolutePath(const std::string &path, const std::string &fileName)
{
    std::string normalizedPath = EnsureTrailingSlash(path);
    return normalizedPath + fileName;
}

std::string vuprs::AddPath(const std::string &basePath, const std::string &addedPath)
{
    std::string normalizedBase = EnsureTrailingSlash(basePath);
    return normalizedBase + addedPath;
}

bool vuprs::MakeDir(const std::string &dir)
{
    if (dir.empty())
    {
        throw std::runtime_error("in [vuprs::MakeDir] dir empty.");
    }
    
    std::string path;
    std::stringstream ss(dir);
    std::string segment;
    
    while (std::getline(ss, segment, '/'))
    {
        if (segment.empty()) continue;
        
        if (path.empty()) path = segment;
        else path = path + "/" + segment;
        
        if (!PathExist(path))
        {
            if (::mkdir(path.c_str(), 0777) != 0 && errno != EEXIST)
            {
                throw std::runtime_error("in [vuprs::MakeDir] failed to mkdir: " + path);
            }
        }
    }
    return true;
}

bool vuprs::PathExist(const std::string &path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0) return false;
    return S_ISDIR(info.st_mode);
}

bool vuprs::FileExist(const std::string &file)
{
    struct stat info;
    if (stat(file.c_str(), &info) != 0) return false;
    return S_ISREG(info.st_mode);
}
