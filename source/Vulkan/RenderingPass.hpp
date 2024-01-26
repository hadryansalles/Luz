#pragma once

#include "VulkanLayer.h"
#include "GraphicsPipelineManager.hpp"

struct RenderingPass {
    GraphicsPipelineDesc gpoDesc;
    GraphicsPipelineResource gpo;
    std::vector<VkClearColorValue> clearColors;
    VkClearDepthStencilValue clearDepth;
    std::vector<vkw::Image> colorAttachments;
    vkw::Image depthAttachment;
    std::vector<VkRenderingAttachmentInfoKHR> colorAttachInfos;
    VkRenderingAttachmentInfoKHR depthAttachInfo;
    bool createAttachments = true;
};

class RenderingPassManager {
public:
    static inline VkSampler sampler;
    static inline RID nextRID;
    static inline std::vector<vkw::Image> thrash;

    static void Create();
    static void Destroy();
    static void DestroyTrash() {
        thrash.clear();
    }
    static void CreateRenderingPass(RenderingPass& pass);
    static void DestroyRenderingPass(RenderingPass& pass);
};