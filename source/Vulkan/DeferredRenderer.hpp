#pragma once

#include <vulkan/vulkan.h>

#include "RenderingPass.hpp"

namespace DeferredShading {

void Setup();
void Create();
void Destroy();
void BeginRendering(VkCommandBuffer commandBuffer, int numFrame);
void EndRendering(VkCommandBuffer commandBuffer, int numFrame);
void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void EndPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void OnImgui(int numFrame);

}