#pragma once

#include <string>
#include <vector>

class FileManager {
public:
    static std::vector<char> ReadRawBytes(const std::string& filename);
    static std::vector<std::vector<char>> ReadShaders(std::vector<std::string> filenames);
};