#pragma once

#include <string>
#include <filesystem>

#include "ImageManager.hpp"

struct TextureDesc {
    std::filesystem::path path = "";
    void* data                 = nullptr;
    uint32_t width             = 0;
    uint32_t height            = 0;
};

struct TextureResource {
};

void DrawTextureOnImgui(vkw::Image& img);
