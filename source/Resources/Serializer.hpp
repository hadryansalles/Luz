#pragma once

#include "Base.hpp"
#include <json.hpp>
#include "AssetManager.hpp"

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

struct BinaryStorage {
    std::vector<u8> data;

    uint32_t Push(void* ptr, uint32_t size) {
        uint32_t offset = data.size();
        data.resize(data.size() + size);
        memcpy(data.data() + offset, ptr, size);
        return offset;
    }

    void* Get(uint32_t offset) {
        return data.data() + offset;
    }
};

struct Serializer {
    Json& j;
    BinaryStorage& storage;
    AssetManager& manager;
    int dir = 0;
    std::filesystem::path filename;
    inline static constexpr int LOAD = 0;
    inline static constexpr int SAVE = 1;

    Serializer(Json& j, BinaryStorage& storage, int dir, AssetManager& manager)
        : j(j)
        , storage(storage)
        , manager(manager)
        , dir(dir)
    {}

    template<typename T>
    void Serialize(Ref<T>& object) {
        if (dir == LOAD) {
            DEBUG_ASSERT(j.contains("type") && j.contains("name") && j.contains("uuid"), "Object doens't contain required fields.");
            ObjectType type = j["type"];
            std::string name = j["name"];
            UUID uuid = j["uuid"];
            object = std::dynamic_pointer_cast<T>(manager.CreateObject(type, name, uuid));
            Serializer s(j, storage, dir, manager);
            object->Serialize(s);
        } else {
            Serializer s(j, storage, dir, manager);
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
        if (dir == SAVE) {
            uint32_t size = v.size() * sizeof(T);
            uint32_t offset = storage.Push(v.data(), size);
            j[field] = Json::object();
            j[field]["offset"] = offset;
            j[field]["size"] = size;
            //j[field] = EncodeBase64((u8*)v.data(), v.size() * sizeof(T));
        } else if (j.contains(field)) {
            //std::vector<u8> data = DecodeBase64(j[field]);
            uint32_t size = j[field]["size"];
            uint32_t offset = j[field]["offset"];
            v.resize(size / sizeof(T));
            memcpy(v.data(), storage.Get(offset), size);
        }
    }

    template<typename T>
    void VectorRef(const std::string& field, std::vector<T>& v) {
         if (dir == SAVE) {
             Json childrenArray = Json::array();
             for (auto& x : v) {
                 Serializer childSerializer(childrenArray.emplace_back(), storage, dir, manager);
                 childSerializer.Serialize(x);
             }
             j[field] = childrenArray;
         } else if (j.contains(field)) {
             v.reserve(j.size());
             for (auto& value : j[field]) {
                 ObjectType type = j["type"];
                 std::string name = j["name"];
                 UUID uuid = j["uuid"];
                 Serializer childSerializer(value, storage, dir, manager);
                 auto& child = v.emplace_back();
                 childSerializer.Serialize(child);
             }
         }
    }

    template <typename T>
    void Asset(const std::string& field, Ref<T>& object) {
        if (dir == SAVE) {
            if (object) {
                j[field] = object->uuid;
            } else {
                j[field] = 0;
            }
        } else if (j.contains(field) && j[field] != 0) {
            object = manager.Get<T>(j[field]);
        }
    }

    template <typename T>
    void Node(const std::string& field, Ref<T>& node, SceneAsset* scene) {
        if (dir == SAVE) {
            if (node) {
                j[field] = node->uuid;
            } else {
                j[field] = 0;
            }
        } else if (j.contains(field) && j[field] != 0) {
            node = scene->Get<T>(j[field]);
        }
    }
};
