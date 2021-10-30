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
    int                   id;
    std::filesystem::path path;
    ImageResource         image;
    VkSampler             sampler = VK_NULL_HANDLE;
    ImTextureID           imguiTexture;
    TextureDescriptor     descriptor;
};

class TextureManager {
    static inline TextureResource* defaultTexture = nullptr;
    static inline TextureResource* whiteTexture   = nullptr;
    static inline std::vector<TextureResource*> textures;

    static inline float imguiTextureScale = 1.0f;

    static void UpdateTexturesDescriptors();

public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();

    static void CreateTextureDescriptors();
    static void DrawOnImgui(TextureResource* texture);

    static TextureResource* GetTexture(std::filesystem::path);
    static TextureResource* CreateTexture(TextureDesc& desc);
    static inline TextureResource* GetDefaultTexture() { return defaultTexture;     }
    static inline TextureResource* GetWhiteTexture()   { return whiteTexture;       }
};
