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

    static bool IsMeshFile(std::filesystem::path path);
    static std::vector<ModelDesc> LoadMeshFile(std::filesystem::path path);
    static std::vector<ModelDesc> LoadObjFile(std::filesystem::path path);

    static void AddObjFileToScene(std::filesystem::path path, Collection* parent);

    static TextureDesc LoadImageFile(std::filesystem::path path);
};
