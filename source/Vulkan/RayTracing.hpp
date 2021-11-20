#pragma once

namespace RayTracing {

void Create();
void Destroy();
void RayTrace(VkCommandBuffer& commandBuffer);
void CreateBLAS(std::vector<RID>& meshes);
void CreateTLAS();
void SetRecreateTLAS();
void OnImgui();
void UpdateViewport(VkExtent2D ext);

}