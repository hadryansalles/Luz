#pragma once

#include <vulkan/vulkan.h>

#include "BufferManager.hpp"

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
    static void Create(const ImageDesc& desc, ImageResource& res, BufferResource& buffer);
    static void Create(void* data, u32 width, u32 height, u16 channels, u32 mipLevels, ImageResource& res);
    static void Destroy(ImageResource& res);
    static void SetLayout(ImageResource& res, VkImageLayout newLayout);
};
