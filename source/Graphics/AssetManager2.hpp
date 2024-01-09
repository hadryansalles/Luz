#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "Base.hpp"

struct Json
{};

struct Asset {
    std::string name = "Unintialized";
    UUID uuid = 0;
    Asset(const std::string& name, const UUID& uuid);
    virtual ~Asset();
    virtual void Serialize(Json& json, int dir);
};

struct TextureAsset : Asset {
    u8* data;
    int channels;
    int width;
    int height;

    ~TextureAsset();
    virtual void Serialize(Json& json, int dir);
};

struct AssetManager2 {
    void Serialize(Json& json, int dir);
    void Import(const std::filesystem::path& path);
    bool IsTexture(const std::filesystem::path& path) const;

    template<typename T>
    std::shared_ptr<T> Get(UUID uuid) {
        return std::dynamic_pointer_cast<T>(assets[uuid]);
    }

private:
    void ImportTexture(const std::filesystem::path& path);
    void ImportMesh(const std::filesystem::path& path);

    std::unordered_map<UUID, std::shared_ptr<Asset>> assets;

    template<typename T>
    std::shared_ptr<T> CreateAsset(const std::string& name) {
        return std::make_shared<T>(name, NewUUID());
    }

    UUID nextUUID = 0;
    UUID NewUUID();
};