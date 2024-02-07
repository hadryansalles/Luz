#pragma once

#include <filesystem>
#include "Base.hpp"

struct AssetManager2;

namespace AssetIO {
    UUID Import(const std::filesystem::path& path, AssetManager2& assets);
    UUID ImportTexture(const std::filesystem::path& path, AssetManager2& assets);
    UUID ImportScene(const std::filesystem::path& path, AssetManager2& assets);
    bool IsTexture(const std::filesystem::path& path);
    bool IsScene(const std::filesystem::path& path);
    void WriteFile(const std::filesystem::path& path, const std::string& content);
    std::string ReadFile(const std::filesystem::path& path);
}