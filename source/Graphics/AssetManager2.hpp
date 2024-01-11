#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <json.hpp>

#include "Base.hpp"

template<typename T>
using Ref = std::shared_ptr<T>;
using Json = nlohmann::json;

enum class AssetType {
    Invalid,
    Texture,
    Mesh,
    Material,
    Scene,
};

struct Asset {
    std::string name = "Unintialized";
    UUID uuid = 0;
    AssetType type = AssetType::Invalid;
    virtual ~Asset();
    virtual void Serialize(Json& j, int dir);
};

struct TextureAsset : Asset {
    std::vector<u8> data;
    int channels;
    int width;
    int height;
    TextureAsset();
    virtual void Serialize(Json& j, int dir);
};

struct MeshAsset : Asset {
    struct MeshVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec2 texCoord;
    };
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;

    MeshAsset();
    virtual void Serialize(Json& j, int dir);
};

struct MaterialAsset : Asset {
    glm::vec3 color = glm::vec4(1.0f);
    glm::vec3 emission = glm::vec3(1.0f);
    f32 metallic = 1;
    f32 roughness = 1;
    Ref<TextureAsset> aoMap;
    Ref<TextureAsset> colorMap;
    Ref<TextureAsset> normalMap;
    Ref<TextureAsset> emissionMap;
    Ref<TextureAsset> metallicRoughnessMap;

    MaterialAsset();
    virtual void Serialize(Json& j, int dir);
};

enum class NodeType {
    Node,
    Mesh,
    Light,
};

struct Node {
    UUID uuid = 0;
    NodeType type = NodeType::Node;
    std::string name = "Unintialized";
    std::vector<Ref<Node>> children;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    virtual void Serialize(Json& j, int dir);
    void AddChildren(Ref<Node> node);
};

struct MeshNode : Node {
    Ref<MeshAsset> mesh;
    Ref<MaterialAsset> material;

    virtual void Serialize(Json& j, int dir);
};

struct SceneAsset : Asset {
    std::vector<Ref<Node>> nodes;

    template<typename T>
    Ref<T> Add() {
        Ref<T> node = std::make_shared<T>();
        nodes.push_back(node);
        return node;
    }

    SceneAsset();
    virtual void Serialize(Json& j, int dir);
};

struct AssetManager2 {
    static AssetManager2& Instance();

    void Serialize(Json& j, int dir);
    void Import(const std::filesystem::path& path);

    bool IsTexture(const std::filesystem::path& path) const;
    bool IsScene(const std::filesystem::path& path) const;

    template<typename T>
    Ref<T> Get(UUID uuid) {
        return std::dynamic_pointer_cast<T>(assets[uuid]);
    }

    template<typename T>
    void SerializeAsset(Ref<T>& asset, Json& j, int dir) {
        if (dir == 0 && j.is_number_unsigned()) {
            UUID uuid = j.get<UUID>();
            asset = Get<T>(uuid);
        } else if (dir == 1) {
            j = asset->uuid;
        }
    }

    void OnImgui();

    template<typename T>
    Ref<T> CreateAsset(const std::string& name) {
        UUID uuid = NewUUID();
        Ref<T> a = std::make_shared<T>();
        a->name = name;
        a->uuid = uuid;
        assets[uuid] = a;
        return a;
    }

private:
    void ImportTexture(const std::filesystem::path& path);
    void ImportScene(const std::filesystem::path& path);

    std::unordered_map<UUID, Ref<Asset>> assets;

    UUID nextUUID = 1;
    UUID NewUUID();

    void ImportSceneGLTF(const std::filesystem::path& path);
    void ImportSceneOBJ(const std::filesystem::path& path);
};