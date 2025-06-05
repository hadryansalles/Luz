#pragma once

#include "Base.hpp"
#include "VulkanWrapper.h"
#include "AssetManager.hpp"

#include <unordered_map>

struct GPUSceneImpl;

struct ShadowMapData {
    vkw::Image img;
    bool readable = false;
    int lightIndex = -1;
    vkw::Buffer volumeBuffer;
    vkw::Buffer volumeIndexBuffer;
    uint32_t volumeIndexCount = 0;
};

struct GPUMesh {
    vkw::Buffer vertexBuffer;
    vkw::Buffer indexBuffer;
    u32 vertexCount;
    u32 indexCount;
    vkw::BLAS blas;
};

struct GPUTexture {
    vkw::Image image;
};

struct GPUModel {
    GPUMesh mesh;
    uint32_t modelRID;
    Ref<MeshNode> node;
};

struct GPUScene {
    GPUScene();
    ~GPUScene();
    
    void Create();
    void Destroy();

    void AddMesh(const Ref<MeshAsset>& asset);
    void AddTexture(const Ref<TextureAsset>& asset);
    void AddAssets(const AssetManager& assets);
    void ClearAssets();

    bool AnyVolumetricLight();

    void UpdateResources(const Ref<SceneAsset>& asset, const Ref<CameraNode>& camera);
    void UpdateResourcesGPU();
    void UpdateLineResources();

    std::vector<GPUModel>& GetMeshModels();
    ShadowMapData& GetShadowMap(UUID uuid);

    uint32_t GetPolygonalMemory();
    uint32_t GetFroxelMemory();

    void SetSwapChainPolygonalMemory(uint32_t memory);
    void SetSwapChainFroxelMemory(uint32_t memory);

    RID GetSceneBuffer();
    RID GetModelsBuffer();
    RID GetFontBitmap();
    vkw::Buffer GetLinesBuffer();

private:
    GPUSceneImpl* impl;
};