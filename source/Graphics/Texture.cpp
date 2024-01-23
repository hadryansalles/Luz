#include "Luzpch.hpp"

#include "Texture.hpp"
#include "GraphicsPipelineManager.hpp"
#include "VulkanLayer.h"

#include "imgui/imgui_impl_vulkan.h"

void DrawTextureOnImgui(vkw::Image& img) {
    float hSpace = ImGui::GetContentRegionAvail().x/2.5f;
    f32 maxSize = std::max(img.width, img.height);
    ImVec2 size = ImVec2((f32)img.width/maxSize, (f32)img.height/maxSize);
    size = ImVec2(size.x*hSpace, size.y * hSpace);
    ImGui::Image(img.imguiRID, size);
}
