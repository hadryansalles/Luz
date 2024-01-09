#include "Luzpch.hpp"

#include "AssetManager2.hpp"

#include <stb_image.h>

Asset::~Asset()
{}

void Asset::Serialize(Json& json, int dir) {

}

TextureAsset::~TextureAsset() {
    delete data;
}

void AssetManager2::Import(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (IsTexture(path)) {
        ImportTexture(path);
    }
}

void AssetManager2::ImportTexture(const std::filesystem::path& path) {
    auto t = CreateAsset<TextureAsset>(path.stem().string());
    t->data = stbi_load(path.string().c_str(), &t->width, &t->height, &t->channels, 4);
}

// obj, gltf -> multple meshes
void AssetManager2::ImportMesh(const std::filesystem::path& path) {
    // for each mesh inside file
        // create new mesh asset
        // set vertices and indicies
}

bool AssetManager2::IsTexture(const std::filesystem::path& path) const {
    const std::string ext = path.extension().string();
    return ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}

UUID AssetManager2::NewUUID() {
    return nextUUID++;
}