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

struct Collection;

struct ModelDesc {
    Collection* collection = nullptr;
    MeshDesc* mesh = nullptr;
    TextureDesc texture;
};

struct Model {
    std::string name;
    Transform transform;
    Collection* collection = nullptr;
    MeshResource* mesh = nullptr;
    TextureResource* texture = nullptr;
    BufferDescriptor meshDescriptor;
    ModelUBO ubo;
    unsigned int id;
};

struct Collection {
    Collection* parent = nullptr;
    std::vector<Model*> models;
    std::vector<Collection*> children;
    Transform transform;
    std::string name;
};
