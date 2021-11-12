#pragma once

#include <filesystem>
#include <mutex>

#include "Texture.hpp"

struct MeshVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    bool operator==(const MeshVertex& other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
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
        attributeDescriptions[1].offset   = offsetof(MeshVertex, normal);

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
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct MeshDesc {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::filesystem::path path;
    std::string name;
    glm::vec3 center = glm::vec3(0, 0, 0);
};

struct MeshResource {
    BufferResource vertexBuffer;
    BufferResource indexBuffer;
    uint32_t indexCount;
};

struct ModelDesc {
    std::string name;
    RID mesh;
    RID texture;
};

#define MAX_MESHES 2048
#define MAX_TEXTURES 2048

class AssetManager {
    static inline RID nextMeshRID = 0;
    static inline std::vector<RID> unintializedMeshes;
    static inline std::mutex meshesLock;

    static inline RID nextTextureRID = 0;
    static inline std::vector<RID> unintializedTextures;
    static inline std::mutex texturesLock;

    static inline std::vector<ModelDesc> loadedModels;
    static inline std::mutex loadedModelsLock;

    static RID NewMesh();
    static void InitializeMesh(RID id);
    static void RecenterMesh(RID id);

    static RID NewTexture();
    static void InitializeTexture(RID id);
    static void UpdateTexturesDescriptor(std::vector<RID>& rids);

    static void LoadOBJ(std::filesystem::path path);

public:
    static inline MeshDesc meshDescs[MAX_MESHES];
    static inline MeshResource meshes[MAX_MESHES];

    static inline TextureDesc textureDescs[MAX_TEXTURES];
    static inline TextureResource textures[MAX_TEXTURES];

    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static void UpdateResources();

    static bool IsOBJ(std::filesystem::path path);
    static bool IsTexture(std::filesystem::path path);

    static RID LoadTexture(std::filesystem::path path);

    static void AsyncLoadModels(std::filesystem::path path);
    static std::vector<ModelDesc> LoadModels(std::filesystem::path path);
    static std::vector<ModelDesc> GetLoadedModels();
};
