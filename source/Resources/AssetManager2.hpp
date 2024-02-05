#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "Base.hpp"
#include "Util.hpp"

struct Serializer;

template<typename T>
using Ref = std::shared_ptr<T>;

enum class ObjectType {
    Invalid,
    TextureAsset,
    MeshAsset,
    MaterialAsset,
    SceneAsset,
    Node,
    MeshNode,
};

inline const char* ObjectTypeName[] = {
    "Invalid",
    "TextureAsset",
    "MeshAsset",
    "MaterialAsset",
    "SceneAsset",
    "Node",
    "MeshNode"
};

struct Object {
    std::string name = "Unintialized";
    UUID uuid = 0;
    ObjectType type = ObjectType::Invalid;

    virtual ~Object();
    virtual void Serialize(Serializer& s) = 0;
};

struct Asset : Object {
    virtual ~Asset();
    virtual void Serialize(Serializer& s) = 0;
};

struct TextureAsset : Asset {
    std::vector<u8> data;
    int channels = 0;
    int width = 0;
    int height = 0;

    TextureAsset();
    virtual void Serialize(Serializer& s);
};

struct MeshAsset : Asset {
    struct MeshVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec2 texCoord;
        bool operator==(const MeshVertex& o) const {
            return position == o.position && normal == o.normal && texCoord == o.texCoord;
        }
    };
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;

    MeshAsset();
    virtual void Serialize(Serializer& s);
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
    virtual void Serialize(Serializer& s);
};

struct Node : Object {
    std::vector<Ref<Node>> children;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 parentTransform = glm::mat4(1.0f);
    glm::mat4 worldTransform = glm::mat4(1.0f);

    Node();
    virtual void Serialize(Serializer& s);

    template<typename T>
    void GetAll(ObjectType type, std::vector<Ref<T>>& all) {
        for (auto& node : children) {
            if (node->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(node));
            }
            node->GetAll(type, all);
        }
    }

    glm::mat4 GetLocalTransform();
    glm::mat4 GetWorldTransform();
    void UpdateTransforms();
};

struct MeshNode : Node {
    Ref<MeshAsset> mesh;
    Ref<MaterialAsset> material;

    MeshNode();
    virtual void Serialize(Serializer& s);
};

struct SceneAsset : Asset {
    std::vector<Ref<Node>> nodes;

    template<typename T>
    Ref<T> Add() {
        Ref<T> node = std::make_shared<T>();
        nodes.push_back(node);
        return node;
    }

    template<typename T>
    void GetAll(ObjectType type, std::vector<Ref<T>>& all) {
        for (auto& node : nodes) {
            if (node->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(node));
            }
            node->GetAll(type, all);
        }
    }

    SceneAsset();
    virtual void Serialize(Serializer& s);
    void UpdateTransforms();
};

struct AssetManager2 {
    static AssetManager2& Instance();

    void LoadProject(const std::filesystem::path& path);
    void SaveProject(const std::filesystem::path& path);
    Ref<SceneAsset> GetInitialScene();
    void OnImgui();

    template<typename T>
    Ref<T> Get(UUID uuid) {
        return std::dynamic_pointer_cast<T>(assets[uuid]);
    }

    Ref<Asset> Get(UUID uuid) {
        return assets[uuid];
    }

    template<typename T>
    std::vector<Ref<T>> GetAll(ObjectType type) {
        std::vector<Ref<T>> all;
        for (auto& pair : assets) {
            if (pair.second->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(pair.second));
            }
        }
        return all;
    }

    template<typename T>
    Ref<T> CreateObject(const std::string& name, UUID uuid = 0) {
        if (uuid == 0) {
            uuid = NewUUID();
        }
        Ref<T> a = std::make_shared<T>();
        a->name = name;
        a->uuid = uuid;
        return a;
    }

    template<typename T>
    Ref<T> CreateAsset(const std::string& name, UUID uuid = 0) {
        auto a = CreateObject<T>(name, uuid);
        assets[a->uuid] = a;
        if (a->type == ObjectType::SceneAsset && !initialScene) {
            initialScene = a->uuid;
        }
        return a;
    }

    Ref<Object> CreateObject(ObjectType type, const std::string& name, UUID uuid = 0) {
        switch (type) {
            case ObjectType::TextureAsset: return CreateAsset<TextureAsset>(name, uuid);
            case ObjectType::MaterialAsset: return CreateAsset<MaterialAsset>(name, uuid);
            case ObjectType::MeshAsset: return CreateAsset<MeshAsset>(name, uuid);
            case ObjectType::SceneAsset: return CreateAsset<SceneAsset>(name, uuid);
            case ObjectType::Node: return CreateObject<Node>(name, uuid);
            case ObjectType::MeshNode: return CreateObject<MeshNode>(name, uuid);
            default: DEBUG_ASSERT(false, "Invalid object type {}.", type) return nullptr;
        }
    }

private:
    std::unordered_map<UUID, Ref<Asset>> assets;
    UUID NewUUID();
    UUID initialScene;
};