#pragma once

#include <string>

#include "BufferManager.hpp"
#include "MeshManager.hpp"
#include "TextureManager.hpp"
#include "Transform.hpp"
#include "Descriptors.hpp"

struct ModelUBO {
    glm::mat4 model = glm::mat4(1.0f);
};

struct ModelDesc {
    MeshDesc* mesh;
    TextureDesc texture;
};

struct Model {
    std::string name;
    Transform transform;
    MeshResource* mesh = nullptr;
    TextureResource* texture = nullptr;
    BufferDescriptor meshDescriptor;
    TextureDescriptor materialDescriptor;
    ModelUBO ubo;
    unsigned int id;
};
