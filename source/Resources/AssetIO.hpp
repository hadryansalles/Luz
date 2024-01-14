#pragma once

#include <filesystem>

namespace AssetIO {
    void Import(const std::filesystem::path& path);
    void ImportTexture(const std::filesystem::path& path);
    void ImportScene(const std::filesystem::path& path);
    bool IsTexture(const std::filesystem::path& path);
    bool IsScene(const std::filesystem::path& path);
    void WriteFile(const std::filesystem::path& path, const std::string& content);
    std::string ReadFile(const std::filesystem::path& path);
}