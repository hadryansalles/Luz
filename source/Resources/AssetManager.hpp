#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "Base.hpp"
#include "Util.hpp"

struct Serializer;
struct AssetManager;

enum class ObjectType {
    Invalid,
    TextureAsset,
    MeshAsset,
    MaterialAsset,
    SceneAsset,
    Node,
    MeshNode,
    LightNode,
    Count,
};

inline std::string ObjectTypeName[] = {
    "Invalid",
    "Texture",
    "Mesh",
    "Material",
    "Scene",
    "Node",
    "MeshNode",
    "LightNode",
    "Count",
};

struct Object {
    std::string name = "Unintialized";
    UUID uuid = 0;
    ObjectType type = ObjectType::Invalid;
    // todo: rethink this gpu dirty flag...
    bool gpuDirty = true;

    Object& operator=(Object& rhs) {
        name = rhs.name;
        type = rhs.type;
        gpuDirty = true;
        return *this;
    }

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
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec3 emission = glm::vec3(0.0f);
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
    Ref<Node> parent;
    std::vector<Ref<Node>> children;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

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

    template<typename T>
    std::vector<Ref<T>> GetAll(ObjectType type) {
        std::vector<Ref<T>> all;
        for (auto& node : children) {
            if (node->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(node));
            }
            node->GetAll(type, all);
        }
        return all;
    }

    static void SetParent(const Ref<Node>& child, const Ref<Node>& parent) {
        if (child->parent) {
            Ref<Node> oldParent = child->parent;
            auto it = std::find_if(oldParent->children.begin(), oldParent->children.end(), [&](auto& n) {
                return child->uuid == n->uuid;
            });
            DEBUG_ASSERT(it != oldParent->children.end(), "Child not found in children vector");
            oldParent->children.erase(it);
        }
        child->parent = parent;
        parent->children.push_back(child);
    }

    static void UpdateChildrenParent(Ref<Node>& node) {
        for (auto& child : node->children) {
            SetParent(child, node);
            UpdateChildrenParent(child);
        }
    }

    static Ref<Node> Clone(Ref<Node>& node);

    glm::mat4 GetLocalTransform();
    glm::mat4 GetWorldTransform();
    glm::vec3 GetWorldPosition();
    glm::mat4 GetParentTransform();
    static glm::mat4 ComposeTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl, glm::mat4 parent = glm::mat4(1));
};

struct MeshNode : Node {
    Ref<MeshAsset> mesh;
    Ref<MaterialAsset> material;

    MeshNode();
    virtual void Serialize(Serializer& s);
};

struct LightNode : Node {
    enum LightType {
        Point = 0,
        Spot = 1,
        Directional = 2
    };
    inline static const char* typeNames[] = { "Point", "Spot", "Directional" };
    glm::vec3 color = glm::vec3(1);
    float intensity = 10.0f;
    LightType lightType = LightType::Point;
    bool shadows = true;
    float radius = 2.0f;
    float innerAngle = 2.0;
    float outerAngle = 2.0;

    LightNode();
    virtual void Serialize(Serializer& s);
};

struct CameraNode : Node {

};

struct SceneAsset : Asset {
    std::vector<Ref<Node>> nodes;
    glm::vec3 ambientLightColor = glm::vec3(1);
    float ambientLight = 0.01f;
    uint32_t aoSamples = 32;
    uint32_t lightSamples = 16;
    float aoMin = 0.0001f;
    float aoMax = 1.0000f;
    float exposure = 2.0f;

    template<typename T>
    Ref<T> Add() {
        Ref<T> node = std::make_shared<T>();
        nodes.push_back(node);
        return node;
    }

    void Add(const Ref<Node>& node) {
        nodes.push_back(node);
    }

    void DeleteRecursive(const Ref<Node>& node);

    template<typename T>
    void GetAll(ObjectType type, std::vector<Ref<T>>& all) {
        for (auto& node : nodes) {
            if (node->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(node));
            }
            node->GetAll(type, all);
        }
    }

    template<typename T>
    std::vector<Ref<T>> GetAll(ObjectType type) {
        std::vector<Ref<T>> all;
        for (auto& node : nodes) {
            if (node->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(node));
            }
            node->GetAll(type, all);
        }
        return all;
    }

    void UpdateParents() {
        for (auto& node : nodes) {
            node->parent = {};
            Node::UpdateChildrenParent(node);
        }
    }

    SceneAsset();
    virtual void Serialize(Serializer& s);
};

struct AssetManager {
    std::vector<Ref<Node>> AddAssetsToScene(Ref<SceneAsset>& scene, const std::vector<std::string>& paths);
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
    std::vector<Ref<T>> GetAll(ObjectType type) const {
        std::vector<Ref<T>> all;
        for (auto& pair : assets) {
            if (pair.second->type == type) {
                all.emplace_back(std::dynamic_pointer_cast<T>(pair.second));
            }
        }
        return all;
    }

    std::vector<Ref<Asset>> GetAll() const {
        std::vector<Ref<Asset>> all;
        for (auto& pair : assets) {
            all.emplace_back(std::dynamic_pointer_cast<Asset>(pair.second));
        }
        return all;
    }

    template<typename T>
    static Ref<T> CreateObject(const std::string& name, UUID uuid = 0) {
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
        if (uuid == 0) {
            uuid = NewUUID();
        }
        Ref<T> a = std::make_shared<T>();
        a->name = name;
        a->uuid = uuid;
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
            case ObjectType::LightNode: return CreateObject<LightNode>(name, uuid);
            default: DEBUG_ASSERT(false, "Invalid object type {}.", type) return nullptr;
        }
    }

    template<typename T>
    Ref<T> CloneAsset(const Ref<Object>& rhs) {
        Ref<T> asset = CreateAsset<T>(rhs->name, 0);
        *asset = *std::dynamic_pointer_cast<T>(rhs);
        return asset;
    }

    template<typename T>
    static Ref<T> CloneObject(const Ref<Object>& rhs) {
        Ref<T> object = CreateObject<T>(rhs->name, 0);
        *object = *std::dynamic_pointer_cast<T>(rhs);
        return object;
    }

    Ref<Object> CloneAsset(ObjectType type, const Ref<Object>& rhs) {
        switch (type) {
            case ObjectType::SceneAsset: return CloneAsset<SceneAsset>(rhs);
            default: DEBUG_ASSERT(false, "Invalid asset type {}.", type) return nullptr;
        }
    }

    static Ref<Object> CloneObject(ObjectType type, const Ref<Object>& rhs) {
        switch (type) {
            case ObjectType::Node: return CloneObject<Node>(rhs);
            case ObjectType::MeshNode: return CloneObject<MeshNode>(rhs);
            case ObjectType::LightNode: return CloneObject<LightNode>(rhs);
            default: DEBUG_ASSERT(false, "Invalid object type {}.", type) return nullptr;
        }
    }

private:
    std::unordered_map<UUID, Ref<Asset>> assets;
    static UUID NewUUID();
    UUID initialScene = 0;
};