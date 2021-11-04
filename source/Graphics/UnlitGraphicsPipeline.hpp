#pragma once

#include "GraphicsPipelineManager.hpp"

struct UnlitMaterialUBO {
    glm::vec4 color;
};

class UnlitGraphicsPipeline {
    static inline GraphicsPipelineDesc desc{};
    static inline GraphicsPipelineResource res{};
public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void OnImgui();

    static inline bool IsDirty()                          { return res.dirty; }
    static inline GraphicsPipelineResource& GetResource() { return res;       }

    static BufferDescriptor CreateModelDescriptor();
    static BufferDescriptor CreateMaterialDescriptor();
};
