#pragma once

namespace RayTracing {

void Create();
void RayTrace(VkCommandBuffer& commandBuffer);
void OnImgui();
void UpdateViewport(VkExtent2D ext);

}