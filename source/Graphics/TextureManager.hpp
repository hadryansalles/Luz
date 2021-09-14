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
    std::filesystem::path path;
    ImageResource         image;
    VkSampler             sampler = VK_NULL_HANDLE;
    ImTextureID           imguiTexture;
};

class TextureManager {
    static inline TextureResource* defaultTexture = nullptr;
    static inline std::vector<TextureResource*> textures;

    static inline float imguiTextureScale = 1.0f;
public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static void CreateImguiTextureDescriptors();

    static TextureResource* CreateTexture(TextureDesc& desc);
    static inline TextureResource* GetDefaultTexture() { return defaultTexture; }
};
