#pragma once

#include "Base.hpp"
#include "VulkanWrapper.h"
#include "AssetManager2.hpp"

#include <unordered_map>

struct GPUSceneImpl;

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
    void AddAssets(const AssetManager2& assets);
    void ClearAssets();

    void UpdateResources(Ref<SceneAsset>& asset);
    void UpdateResourcesGPU();

    std::vector<GPUModel>& GetMeshModels();

    RID GetSceneBuffer();
    RID GetModelsBuffer();

private:
    GPUSceneImpl* impl;
};