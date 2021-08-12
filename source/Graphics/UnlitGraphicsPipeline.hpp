#pragma once

#include "GraphicsPipelineManager.hpp"

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
};
