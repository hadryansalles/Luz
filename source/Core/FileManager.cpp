#include "Luzpch.hpp"

#include "FileManager.hpp"

std::vector<char> FileManager::ReadRawBytes(const std::string& filename) {
    // 'ate' specify to start reading at the end of the file
    // then we can use the read position to determine the size of the file
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        LOG_CRITICAL("Failed to open file: '{}'", filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}