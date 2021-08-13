#pragma once

#include <filesystem>

#include "Model.hpp"
#include "MeshManager.hpp"
#include "TextureManager.hpp"

class AssetManager {
private:
public:
    static void Create();
    static void Destroy();
    static void Load(std::filesystem::path path);
    static std::vector<Model*> LoadObjFile(std::filesystem::path path);
    static TextureResource* LoadImageFile(std::filesystem::path path);
};
