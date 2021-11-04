#pragma once

#include "GraphicsPipelineManager.hpp"

struct PhongMaterialUBO {
    glm::vec4 diffuseColor;
    glm::vec4 specularColor;
};

class PhongGraphicsPipeline {
    static inline GraphicsPipelineDesc desc{};
    static inline GraphicsPipelineResource res{};
public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void OnImgui();

    static inline bool IsDirty() { return res.dirty; }
    static inline GraphicsPipelineResource& GetResource() { return res; }

    static BufferDescriptor CreateSceneDescriptor();
    static BufferDescriptor CreateModelDescriptor();
    static BufferDescriptor CreateMaterialDescriptor();
    static TextureDescriptor CreateTextureDescriptor();
};
