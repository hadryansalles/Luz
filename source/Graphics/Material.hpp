#pragma once

#include <glm.hpp>

#include "TextureManager.hpp"

enum class MaterialType {
    Unlit,
    Phong,
};

struct Material {
    glm::vec4 diffuseColor = glm::vec4(1);
    glm::vec4 specularColor = glm::vec4(1);

    MaterialType type = MaterialType::Phong;

    RID diffuseTexture = 0;
    bool useDiffuseTexture = true;

    BufferDescriptor materialDescriptor;
};

// #pragma once

// #include <glm.hpp>

// #include "TextureManager.hpp"

// struct UnlitMaterial {
//     glm::vec4 color = glm::vec4(1.0f);
//     RID colorTexture = 0;
// };

// struct PhongMaterial {
//     glm::vec4 color = glm::vec4(1.0f);
//     RID colorTexture = 0;
// };

// enum MaterialType {
//     Phong,
//     Unlit,
// };

// struct Material {
//     std::string name = "Material";
//     glm::vec4 diffuseColor = glm::vec4(1);
//     glm::vec4 specularColor = glm::vec4(1);

//     MaterialType type = MaterialType::Phong;

//     RID diffuseTexture = 0;
//     bool useDiffuseTexture = true;

//     BufferDescriptor materialDescriptor;
// };
