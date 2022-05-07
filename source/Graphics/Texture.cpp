#include "Luzpch.hpp"

#include "Texture.hpp"
#include "Instance.hpp"
#include "AssetManager.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "GraphicsPipelineManager.hpp"
#include "DeferredRenderer.hpp"

#include "imgui/imgui_impl_vulkan.h"

void CreateTextureResource(TextureDesc& desc, TextureResource& res) {
    u32 mipLevels = (u32)(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
    ImageManager::Create(desc.data, desc.width, desc.height, 4, mipLevels, res.image);
    res.sampler = CreateSampler(mipLevels);
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    res.imguiRID = ImGui_ImplVulkan_AddTexture(res.sampler, res.image.view, layout);
}

void CreateHDRCubeTextureResource(TextureDesc& desc, TextureResource& res, RID resID) {
    u32 faceSize = 2048;
    ImageResource hdriImage;
    {
        ImageDesc imageDesc;
        imageDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageDesc.width = faceSize;
        imageDesc.height = faceSize;
        imageDesc.layers = 6;
        imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageManager::Create(imageDesc, hdriImage);
    }
    {
        ImageDesc imageDesc;
        imageDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        imageDesc.width = desc.width;
        imageDesc.height = desc.height;
        imageDesc.layers = 1;
        imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        imageDesc.size = sizeof(float) * desc.width * desc.height * 4;
        imageDesc.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
        BufferResource staging;
        BufferManager::CreateStagingBuffer(staging, desc.data, imageDesc.size);
        ImageManager::Create(imageDesc, res.image, staging);
        BufferManager::Destroy(staging);
    }
    res.sampler = CreateSampler(1);
    std::vector<RID> toUpdate({ resID });
    AssetManager::UpdateTexturesDescriptor(toUpdate);
    ImageResource faces[6];
    auto cmdBuffer = LogicalDevice::BeginSingleTimeCommands();
    for (int i = 0; i < 6; i++) {
        ImageDesc imageDesc;
        imageDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageDesc.width = faceSize;
        imageDesc.height = faceSize;
        imageDesc.layers = 1;
        imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageManager::Create(imageDesc, faces[i]);
        DeferredShading::RenderPanoramaToFace(cmdBuffer, resID, faces[i], i);

        VkImageCopy region{};
        region.srcOffset = { 0, 0, 0 };
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcSubresource.mipLevel = 0;
        region.dstOffset = { 0, 0, 0 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = i;
        region.dstSubresource.layerCount = 1;
        region.dstSubresource.mipLevel = 0;
        region.extent = { faceSize, faceSize, 1 };
        ImageManager::Copy(cmdBuffer, faces[i], hdriImage, region);
    }
    LogicalDevice::EndSingleTimeCommands(cmdBuffer);
    ImageManager::Destroy(res.image);
    for (int i = 0; i < 6; i++) {
        ImageManager::Destroy(faces[i]);
    }
    DeferredShading::panoramaRID = resID;
    res.image = hdriImage;
    AssetManager::UpdateTexturesDescriptor(toUpdate);
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    res.imguiRID = ImGui_ImplVulkan_AddTexture(res.sampler, res.image.view, layout);
}

void CreateCubeTextureResource(TextureDesc& desc, TextureResource& res) {
    // int single_size = desc.width * desc.height * 4;
    // unsigned char* data = new unsigned char[single_size * 6];
    // memcpy(&(data[0]), desc.data[0], single_size);
    // memcpy(&(data[single_size * 1]), desc.data[1], single_size);
    // memcpy(&(data[single_size * 2]), desc.data[2], single_size);
    // memcpy(&(data[single_size * 3]), desc.data[3], single_size);
    // memcpy(&(data[single_size * 4]), desc.data[4], single_size);
    // memcpy(&(data[single_size * 5]), desc.data[5], single_size);
    ImageManager::CreateCubeImage(desc.data, desc.width, desc.height, 4, res.image);
    // res.sampler = CreateCubeSampler();
    res.sampler = CreateSampler(1);
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    res.imguiRID = ImGui_ImplVulkan_AddTexture(res.sampler, res.image.view, layout);
}

void DestroyTextureResource(TextureResource& res) {
    ImageManager::Destroy(res.image);
    vkDestroySampler(LogicalDevice::GetVkDevice(), res.sampler, Instance::GetAllocator());
    res.sampler = VK_NULL_HANDLE;
    res.imguiRID = nullptr;
}

void DrawTextureOnImgui(TextureResource& res) {
    float hSpace = ImGui::GetContentRegionAvailWidth()/2.5f;
    f32 maxSize = std::max(res.image.width, res.image.height);
    ImVec2 size = ImVec2((f32)res.image.width/maxSize, (f32)res.image.height/maxSize);
    size = ImVec2(size.x*hSpace, size.y * hSpace);
    ImGui::Image(res.imguiRID, size);
}

VkSampler CreateCubeSampler() {
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
    samplerInfo.maxLod = 1;

    VkSampler sampler = VK_NULL_HANDLE;
    auto vkRes = vkCreateSampler(LogicalDevice::GetVkDevice(), &samplerInfo, nullptr, &sampler);
    DEBUG_VK(vkRes, "Failed to create texture sampler!");

    return sampler;
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
