#pragma once

#include <string>
#include <filesystem>

#include "ImageManager.hpp"

struct TextureDesc {
    std::vector<std::filesystem::path> paths;
    void* data                 = nullptr;
    uint32_t width             = 0;
    uint32_t height            = 0;
    bool isHDR = false;
};

struct TextureResource {
    ImageResource image;
    VkSampler     sampler   = VK_NULL_HANDLE;
    ImTextureID   imguiRID  = nullptr;
};

void CreateTextureResource(TextureDesc& desc, TextureResource& res);
void CreateHDRCubeTextureResource(TextureDesc& desc, TextureResource& res, RID resID);
void CreateCubeTextureResource(TextureDesc& desc, TextureResource& res);
void DestroyTextureResource(TextureResource& res);
void DrawTextureOnImgui(TextureResource& res);
void DrawTextureOnImgui(ImageResource& image, ImTextureID imguiRid, float scale);
VkSampler CreateSampler(f32 maxLod, bool clamp = false);
VkSampler CreateCubeSampler();
