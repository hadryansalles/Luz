#pragma once

#include "ImageManager.hpp"
#include "GraphicsPipelineManager.hpp"

struct RenderingPass {
    GraphicsPipelineDesc gpoDesc;
    GraphicsPipelineResource gpo;
    std::vector<VkClearColorValue> clearColors;
    VkClearDepthStencilValue clearDepth;
    std::vector<RID> colorAttachments;
    RID depthAttachment;
    bool crateAttachments = true;
    std::vector<VkRenderingAttachmentInfoKHR> colorAttachInfos;
    VkRenderingAttachmentInfoKHR depthAttachInfo;
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