#pragma once

#include "ImageManager.hpp"
#include "GraphicsPipelineManager.hpp"

struct RenderingPass {
    int useDepthAttachment = false;
    int numColorAttachments = 0;
    GraphicsPipelineDesc gpoDesc;
    GraphicsPipelineResource gpo;
    std::vector<RID> colorAttachments;
    RID depthAttachment;
};

class RenderingPassManager {
public:
    static inline ImageResource imageAttachments[64];
    static inline VkSampler sampler;
    static inline RID nextRID;

    static void Create();
    static void Destroy();
    static void CreateRenderingPass(RenderingPass& pass);
    static void DestroyRenderingPass(RenderingPass& pass);
};