#pragma once

#include <filesystem>
#include "Base.hpp"

struct AssetManager;

namespace AssetIO {
    UUID Import(const std::filesystem::path& path, AssetManager& assets);
    UUID ImportTexture(const std::filesystem::path& path, AssetManager& assets);
    UUID ImportScene(const std::filesystem::path& path, AssetManager& assets);
    bool IsTexture(const std::filesystem::path& path);
    bool IsScene(const std::filesystem::path& path);
    void WriteFile(const std::filesystem::path& path, const std::string& content);
    std::string ReadFile(const std::filesystem::path& path);
}