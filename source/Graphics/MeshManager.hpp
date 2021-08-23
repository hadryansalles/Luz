#pragma once

#include <array>
#include <filesystem>
#include <stddef.h>
#include <unordered_map>
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "BufferManager.hpp"

struct MeshVertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const MeshVertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

    static VkVertexInputBindingDescription getBindingDescription() {
        // specifies at which rate load data from memory throughout the vertices,
        // number of bytes between data entries 
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(MeshVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        // describe how to extract each vertex attribute from the chunk of data
        // inside the binding description
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(MeshVertex, pos);

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset   = offsetof(MeshVertex, color);

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset   = offsetof(MeshVertex, texCoord);

        return attributeDescriptions;
    }
};

namespace std {
    template<> struct hash<MeshVertex> {
        size_t operator()(MeshVertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct MeshDesc {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::filesystem::path path;
    std::string name;
};

struct MeshResource {
    BufferResource vertexBuffer;
    BufferResource indexBuffer;
    uint32_t indexCount;
};

class MeshManager {
private:
    static inline std::vector<MeshDesc*> descs;
    static inline std::vector<MeshResource*> meshes;

    static void SetupMesh(MeshDesc* desc, MeshResource* res);
public:
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static MeshResource* CreateMesh(MeshDesc* desc);
};
