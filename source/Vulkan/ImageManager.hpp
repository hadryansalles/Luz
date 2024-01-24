#pragma once

#include <vulkan/vulkan.h>
#include "VulkanLayer.h"

class ImageManager {
public:
    static void InsertBarrier (
        VkCommandBuffer commandBuffer, 
        VkImage image, 
        VkAccessFlags srcAccess, 
        VkAccessFlags dstAccess, 
        VkImageLayout oldLayout,
        VkImageLayout newLayout, 
        VkPipelineStageFlags srcStage, 
        VkPipelineStageFlags dstStage, 
        VkImageSubresourceRange subresourceRange
    );

    static void BarrierColorAttachmentToRead(VkCommandBuffer commandBuffer, VkImage image);
    static void BarrierDepthAttachmentToRead(VkCommandBuffer commandBuffer, VkImage image);
    static void BarrierColorAttachmentToPresent(VkCommandBuffer commandBuffer, VkImage image);
    static void BarrierColorUndefinedToAttachment(VkCommandBuffer commandBuffer, VkImage image);
    static void BarrierDepthUndefinedToAttachment(VkCommandBuffer commandBuffer, VkImage image);
};
