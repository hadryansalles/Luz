#include "Luzpch.hpp"

#include "Texture.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "GraphicsPipelineManager.hpp"

#include "imgui/imgui_impl_vulkan.h"

void CreateTextureResource(TextureDesc& desc, TextureResource& res) {
    u32 mipLevels = (u32)(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
    ImageManager::Create(desc.data, desc.width, desc.height, 4, mipLevels, res.image);
    res.sampler = CreateSampler(mipLevels);
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    res.imguiRID = ImGui_ImplVulkan_AddTexture(res.sampler, res.image.view, layout);
}

void DestroyTextureResource(TextureResource& res) {
    ImageManager::Destroy(res.image);
    vkDestroySampler(LogicalDevice::GetVkDevice(), res.sampler, Instance::GetAllocator());
    res.sampler = VK_NULL_HANDLE;
    res.imguiRID = nullptr;
}

void CreateSamplerAndImgui(u32 mipLevels, TextureResource& res) {
    res.sampler = CreateSampler(mipLevels);
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    res.imguiRID = ImGui_ImplVulkan_AddTexture(res.sampler, res.image.view, layout);
}

void DrawTextureOnImgui(TextureResource& res) {
    float hSpace = ImGui::GetContentRegionAvail().x/2.5f;
    f32 maxSize = std::max(res.image.width, res.image.height);
    ImVec2 size = ImVec2((f32)res.image.width/maxSize, (f32)res.image.height/maxSize);
    size = ImVec2(size.x*hSpace, size.y * hSpace);
    ImGui::Image(res.imguiRID, size);
}

VkSampler CreateSampler(f32 maxLod) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    // get the max ansotropy level of my device
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(PhysicalDevice::GetVkPhysicalDevice(), &properties);

    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    // what color to return when clamp is active in addressing mode
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    // if comparison is enabled, texels will be compared to a value an the result 
    // is used in filtering operations, can be used in PCF on shadow maps
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = maxLod;

    VkSampler sampler = VK_NULL_HANDLE;
    auto vkRes = vkCreateSampler(LogicalDevice::GetVkDevice(), &samplerInfo, nullptr, &sampler);
    DEBUG_VK(vkRes, "Failed to create texture sampler!");

    return sampler;
}
