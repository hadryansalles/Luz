#pragma once

#include "Base.hpp"
#include "VulkanWrapper.h"

struct GPUMesh {
    vkw::Buffer vertexBuffer;
    vkw::Buffer indexBuffer;
    u32 vertexCount;
    u32 indexCount;
    vkw::BLAS blas;
}

struct GPUModel {

}

struct GPUScene {
    void UpdateResources()
    std::vector<GPUMesh> meshes;
}