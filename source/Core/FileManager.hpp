#pragma once

#include <string>
#include <vector>

class FileManager {
public:
    static std::vector<char> ReadRawBytes(const std::string& filename);
    static int GetFileVersion(const std::string& filename);
};