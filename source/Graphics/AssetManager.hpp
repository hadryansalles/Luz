#pragma once

#include <filesystem>
#include <mutex>

#include "Scene.hpp"
#include "VulkanWrapper.h"

struct TextureDesc {
    std::filesystem::path path = "";
    void* data                 = nullptr;
    uint32_t width             = 0;
    uint32_t height            = 0;
};

struct MeshVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 texCoord;

    bool operator==(const MeshVertex& other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
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
    i32 materialIndex;
};

struct MeshResource {
    vkw::Buffer vertexBuffer;
    vkw::Buffer indexBuffer;
    u32 vertexCount;
    u32 indexCount;
    vkw::BLAS blas;
};

#define MAX_MESHES 2048
#define MAX_TEXTURES 2048

class AssetManager {
    static inline std::vector<RID> unintializedMeshes;
    static inline std::mutex meshesLock;

    static inline RID nextTextureRID = 0;
    static inline std::vector<RID> unintializedTextures;
    static inline std::mutex texturesLock;

    static inline std::vector<Model> loadedModels;
    static inline std::mutex loadedModelsLock;

    static RID NewMesh();
    static void InitializeMesh(RID id);
    static void RecenterMesh(RID id);

    static RID NewTexture();
    static void InitializeTexture(RID id);
    static void UpdateTexturesDescriptor(std::vector<RID>& rids);

    static Entity* LoadOBJ(std::filesystem::path path, Scene& scene);
    static Entity* LoadGLTF(std::filesystem::path path, Scene& scene);

    static bool IsOBJ(std::filesystem::path path);
    static bool IsGLTF(std::filesystem::path path);

public:
    static inline RID nextMeshRID = 0;

    static inline MeshDesc meshDescs[MAX_MESHES];
    static inline MeshResource meshes[MAX_MESHES];

    static inline TextureDesc textureDescs[MAX_TEXTURES];

    static inline vkw::Image images[MAX_TEXTURES];

    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static void UpdateResources(Scene& scene);

    static bool IsModel(std::filesystem::path path);
    static bool IsTexture(std::filesystem::path path);

    static RID CreateTexture(std::string name, u8* data, u32 width, u32 height);
    static RID LoadTexture(std::filesystem::path path);

    static void SaveGLTF(const std::filesystem::path& path, const Scene& scene);
    static Entity* LoadModel(std::filesystem::path path, Scene& scene);
};
