#pragma once

#include <string>

#include "ImageManager.hpp"

struct TextureDesc {
    void* data;
    uint32_t width;
    uint32_t height;
};

struct TextureResource {
    std::string   name;
    ImageResource image;
    VkSampler     sampler;
};

class TextureManager {
    static inline std::vector<TextureResource*> textures;
public:
    static void Create();
    static void Destroy();
    static TextureResource* CreateTexture(TextureDesc& desc);
};
