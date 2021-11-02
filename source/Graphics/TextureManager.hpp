#pragma once

#include <string>
#include <filesystem>

#include "ImageManager.hpp"
#include "Descriptors.hpp"

struct TextureDesc {
    void* data = nullptr;
    std::filesystem::path path;
    uint32_t width;
    uint32_t height;
};

struct TextureResource {
    ImageResource         image;
    std::filesystem::path path      = "Invalid";
    VkSampler             sampler   = VK_NULL_HANDLE;
    ImTextureID           imguiRID  = nullptr;
};

class TextureManager {
public:
    static inline constexpr int MAX_TEXTURES = 2048;

private:
    static inline float imguiTextureScale = 1.0f;

    static inline TextureResource resources[MAX_TEXTURES];
    static inline RID             nextRID = 0;

    static void InitTexture(RID rid, TextureDesc& desc);
    static void UpdateDescriptor(RID first, RID last);

public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static void DrawOnImgui(RID textureRID);

    static RID GetTexture(std::filesystem::path);
    static RID CreateTexture(TextureDesc& desc);
    static VkSampler CreateSampler(f32 maxLod);
};
