#pragma once

#include <vulkan/vulkan.h>
#include "VulkanLayer.h"

struct ImageResource {
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct ImageDesc {
    VkFormat format;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkSampleCountFlagBits numSamples;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkImageAspectFlags aspect;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDeviceSize size;

    uint32_t width;
    uint32_t height;
    uint32_t mipLevels = 1;
};

class ImageManager {
public:
    static void Create(const ImageDesc& desc, ImageResource& res);
    static void Create(const ImageDesc& desc, ImageResource& res, vkw::Buffer& buffer);
    static void Create(void* data, u32 width, u32 height, u16 channels, u32 mipLevels, ImageResource& res);
    static void Destroy(ImageResource& res);

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
