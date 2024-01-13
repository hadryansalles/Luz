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

inline void to_json(Json& j, const glm::vec3& v) {
    j = Json{v.x, v.y, v.z};
}

inline void to_json(Json& j, const glm::vec4& v) {
    j = Json{v.x, v.y, v.z, v.w};
}

inline void from_json(const Json& j, glm::vec4& v) {
    if (j.is_array() && j.size() == 4) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
        v.w = j[3];
    }
}

inline void from_json(const Json& j, glm::vec3& v) {
    if (j.is_array() && j.size() == 3) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
    }
}

struct Serializer;

// todo: move to proper place
std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

enum class ObjectType {
    Invalid,
    TextureAsset,
    MeshAsset,
    MaterialAsset,
    SceneAsset,
    Node,
    MeshNode,
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

    Node();
    virtual void Serialize(Serializer& s);
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

    SceneAsset();
    virtual void Serialize(Serializer& s);
};

struct AssetManager2 {
    static AssetManager2& Instance();

    void Serialize(Json& j, int dir);
    void Import(const std::filesystem::path& path);
    void OnImgui();
    void Clear();

    bool IsTexture(const std::filesystem::path& path) const;
    bool IsScene(const std::filesystem::path& path) const;

    template<typename T>
    Ref<T> Get(UUID uuid) {
        return std::dynamic_pointer_cast<T>(assets[uuid]);
    }

    Ref<Asset> Get(UUID uuid) {
        return assets[uuid];
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
        auto& a = CreateObject<T>(name, uuid);
        assets[a->uuid] = a;
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
            default: DEBUG_ASSERT(false, "Invalid object type {}.", type);
        }
    }

private:
    std::unordered_map<UUID, Ref<Asset>> assets;

    UUID NewUUID();
    void ImportTexture(const std::filesystem::path& path);
    void ImportScene(const std::filesystem::path& path);
    void ImportSceneGLTF(const std::filesystem::path& path);
    void ImportSceneOBJ(const std::filesystem::path& path);
};

struct Serializer {
    Json& j;
    int dir = 0;
    inline static constexpr int LOAD = 0;
    inline static constexpr int SAVE = 1;

    Serializer(Json& j, int dir)
        : j(j)
        , dir(dir)
    {}

    template<typename T>
    void Serialize(Ref<T>& object) {
        if (dir == LOAD) {
            DEBUG_ASSERT(j.contains("type") && j.contains("name") && j.contains("uuid"), "Object doens't contain required fields.");
            ObjectType type = j["type"];
            std::string name = j["name"];
            UUID uuid = j["uuid"];
            object = std::dynamic_pointer_cast<T>(AssetManager2::Instance().CreateObject(type, name, uuid));
            Serializer s(j, dir);
            object->Serialize(s);
        } else {
            Serializer s(j, dir);
            j["type"] = object->type;
            j["name"] = object->name;
            j["uuid"] = object->uuid;
            object->Serialize(s);
        }
    }

    template<typename T>
    void operator()(const std::string& field, T& value) {
        if (dir == SAVE) {
            to_json(j[field], value);
        } else if (j.contains(field)) {
            from_json(j[field], value);
        }
    }

    template<typename T>
    void Vector(const std::string& field, std::vector<T>& v) {
        if constexpr (std::is_pod_v<T>) {
            if (dir == SAVE) {
                j[field] = EncodeBase64((u8*)v.data(), v.size() * sizeof(T));
            } else if (j.contains(field)) {
                std::vector<u8> data = DecodeBase64(j[field]);
                v.resize(data.size()/sizeof(T));
                memcpy(v.data(), data.data(), data.size());
            }
        } else {
             if (dir == SAVE) {
                 Json childrenArray = Json::array();
                 for (auto& x : v) {
                     Serializer childSerializer(childrenArray.emplace_back(), dir);
                     childSerializer.Serialize(x);
                 }
                 j[field] = childrenArray;
             } else if (j.contains(field)) {
                 v.reserve(j.size());
                 for (auto& value : j[field]) {
                     ObjectType type = j["type"];
                     std::string name = j["name"];
                     UUID uuid = j["uuid"];
                     Serializer childSerializer(value, dir);
                     childSerializer.Serialize(v.emplace_back());
                 }
             }
        }
    }

    template <typename T>
    void Asset(const std::string& field, Ref<T>& object) {
        if (dir == SAVE) {
            j[field] = object->uuid;
        } else if (j.contains(field)) {
            object = AssetManager2::Instance().Get<T>(j[field]);
        }
    }
};