#pragma once

#include <glm.hpp>

#include "TextureManager.hpp"

enum class MaterialType {
    Unlit,
};

struct Material {
    glm::vec4 diffuseColor = glm::vec4(1);

    MaterialType type = MaterialType::Unlit;

    TextureResource* diffuseTexture = nullptr;
    bool useDiffuseTexture = true;

    BufferDescriptor materialDescriptor;
};

class MaterialManager {
public:
    static void OnImgui(const Material& material);
};
