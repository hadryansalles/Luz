#pragma once

#include <vulkan/vulkan.h>

#include "RenderingPass.hpp"

namespace DeferredShading {

struct OpaqueConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int frameID;
};

inline RenderingPass opaquePass;
inline RenderingPass presentPass;

void Setup();
void Create();
void Destroy();

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId);

void BindConstants(VkCommandBuffer commandBuffer, RenderingPass& pass, void* data, u32 size);
void BeginOpaquePass(VkCommandBuffer commandBuffer, RenderingPass& pass);
void EndPass(VkCommandBuffer commandBuffer);

void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void EndPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void OnImgui(int numFrame);

}