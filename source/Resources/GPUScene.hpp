#pragma once

#include "Base.hpp"
#include "BufferManager.hpp"
#include "Texture.hpp"
#include "Shared.hpp"
#include "Camera.hpp"

struct SceneAsset;
struct MeshNode;

struct GPUMesh {
    uint32_t rid;
    BufferResource vertexBuffer;
    BufferResource indexBuffer;
    u32 vertexCount;
    u32 indexCount;
};

struct GPUTexture {
    TextureResource resource;
    uint32_t rid;
};

struct GPUScene {
    void SetScene(Ref<SceneAsset>& scene);
    void SetCamera(Ref<Camera>& camera);
    void CreateResources();
    void UpdateResources(int numFrame);
    void DestroyResources();
private:
    Ref<SceneAsset> sceneAsset;
    Ref<Camera> camera;
    std::vector<Ref<MeshNode>> meshNodes;
    std::unordered_map<UUID, GPUMesh> meshes;
    std::unordered_map<UUID, GPUTexture> textures;
    StorageBuffer sceneBuffer;
    StorageBuffer modelsBuffer;
};