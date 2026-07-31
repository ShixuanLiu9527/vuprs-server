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
#include <complex>

namespace vuprs
{
    std::string FileAbsolutePath(const std::string &path, const std::string &filename);
    std::string AddPath(const std::string &base_path, const std::string &added_path);
    void SplitFile(const std::string &fullpath, std::string *dir, std::string *filename, std::string *extension);
    bool PathExist(const std::string &path);
    bool FileExist(const std::string &file);
    bool MakeDir(const std::string &dir);

    bool SaveToCSV(const std::vector<double> &data, const std::string &filename);
    bool SaveToCSV(const std::vector<std::vector<double>> &data, const std::string &filename);
    bool SaveToCSV_complex(const std::vector<std::complex<double>> &data, const std::string &filename);
}

#endif