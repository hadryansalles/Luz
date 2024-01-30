#pragma once

#include <vulkan/vulkan.h>
#include "Base.hpp"

namespace DeferredShading {

struct OpaqueConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

struct LightConstants {
    int sceneBufferIndex;
    int frameID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
};

void Setup();
void Recreate(uint32_t width, uint32_t height);
void Destroy();
void ReloadShaders();

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId);

void LightPass(VkCommandBuffer commandBuffer, LightConstants constants);

void BeginOpaquePass(VkCommandBuffer commandBuffer);
void EndPass(VkCommandBuffer commandBuffer);

void BeginPresentPass(VkCommandBuffer commandBuffer);
void EndPresentPass(VkCommandBuffer commandBuffer);
void OnImgui(int numFrame);

}