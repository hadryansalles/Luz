#pragma once

#include <string>
#include <filesystem>

#include "ImageManager.hpp"

struct TextureDesc {
    void* data;
    uint32_t width;
    uint32_t height;
};

struct TextureResource {
    std::filesystem::path path;
    ImageResource         image;
    VkSampler             sampler;
};

class TextureManager {
    static inline TextureResource* defaultTexture;
    static inline std::vector<TextureResource*> textures;
public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Finish();
    static TextureResource* CreateTexture(TextureDesc& desc);
    static inline TextureResource* GetDefaultTexture() { return defaultTexture; }
};
