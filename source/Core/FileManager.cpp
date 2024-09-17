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

int FileManager::GetFileVersion(const std::string& filename) {
    std::filesystem::path filePath(filename);
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File does not exist: '{}'", filename);
        return -1;
    }

    auto lastWriteTime = std::filesystem::last_write_time(filePath);
    auto duration = lastWriteTime.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    return static_cast<int>(seconds);
}