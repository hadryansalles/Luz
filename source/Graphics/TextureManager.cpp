#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "BufferManager.hpp"
#include "TextureManager.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "Instance.hpp"

#include <stb_image.h>

void TextureManager::Setup() {
    defaultTexture = new TextureResource();
    defaultTexture->path = "assets/models/default.png";
}

void TextureManager::Create() {
    LUZ_PROFILE_FUNC();
    TextureResource* newTexture = TextureManager::CreateTexture(AssetManager::LoadImageFile(defaultTexture->path));
    defaultTexture->image = newTexture->image;
    defaultTexture->sampler = newTexture->sampler;
    DEBUG_ASSERT(textures[textures.size() - 1] == newTexture, "New texture is different than last texture on buffer!");
    textures.pop_back();
    for (TextureResource* texture : textures) {
        newTexture = TextureManager::CreateTexture(AssetManager::LoadImageFile(texture->path));
        texture->image = newTexture->image;
        texture->sampler = newTexture->sampler;
        DEBUG_ASSERT(textures[textures.size() - 1] == newTexture, "New texture is different than last texture on buffer!");
        textures.pop_back();
    }
}

void TextureManager::Destroy() {
    ImageManager::Destroy(defaultTexture->image);
    vkDestroySampler(LogicalDevice::GetVkDevice(), defaultTexture->sampler, Instance::GetAllocator());
    for (TextureResource* texture : textures) {
        ImageManager::Destroy(texture->image);
        vkDestroySampler(LogicalDevice::GetVkDevice(), texture->sampler, Instance::GetAllocator());
        texture->sampler = VK_NULL_HANDLE;
    }
}

void TextureManager::Finish() {
    for (TextureResource* texture : textures) {
        delete texture;
    }
    textures.clear();
}

TextureResource* TextureManager::CreateTexture(TextureDesc& desc) {
    for (int i = 0; i < textures.size(); i++) {
        if (desc.path == textures[i]->path && textures[i]->sampler != VK_NULL_HANDLE) {
            return textures[i];
        }
    }
    auto device = LogicalDevice::GetVkDevice();
    auto instance = Instance::GetVkInstance();

    ImageDesc imageDesc{};
    imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    imageDesc.width = desc.width;
    imageDesc.height = desc.height;
    imageDesc.mipLevels = (uint32_t)(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
    imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    imageDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    imageDesc.size = (uint32_t)(desc.width * desc.height * 4);

    TextureResource* res = new TextureResource();

    BufferResource staging;
    BufferManager::CreateStagingBuffer(staging, desc.data, imageDesc.size);
    ImageManager::Create(imageDesc, res->image, staging);
    BufferManager::Destroy(staging);

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
    samplerInfo.maxLod = static_cast<float>(imageDesc.mipLevels);

    auto vkRes = vkCreateSampler(device, &samplerInfo, nullptr, &res->sampler);
    DEBUG_VK(vkRes, "Failed to create texture sampler!");

    textures.push_back(res);

    stbi_image_free(desc.data);
    res->path = desc.path;

    return res;
}
