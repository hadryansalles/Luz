#pragma once

#include <string>
#include <filesystem>

#include "ImageManager.hpp"

struct TextureDesc {
    std::vector<std::filesystem::path> paths;
    void* data                 = nullptr;
    uint32_t width             = 0;
    uint32_t height            = 0;
};

struct TextureResource {
    ImageResource image;
    VkSampler     sampler   = VK_NULL_HANDLE;
    ImTextureID   imguiRID  = nullptr;
};

void CreateTextureResource(TextureDesc& desc, TextureResource& res);
void CreateCubeTextureResource(TextureDesc& desc, TextureResource& res);
void DestroyTextureResource(TextureResource& res);
void DrawTextureOnImgui(TextureResource& res);
VkSampler CreateSampler(f32 maxLod);
VkSampler CreateCubeSampler();
