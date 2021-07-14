#pragma once

#include <vulkan/vulkan.h>

class VulkanImage {
public:
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;

    VkFormat format;
    VkImageTiling tiling;
    VkSampleCountFlagBits numSamples;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags properties;
    VkImageAspectFlags aspect;
    VkImageLayout layout;

    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    
    VulkanImage();
    void Create(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                VkImageTiling tiling, VkSampleCountFlagBits numSamples, VkImageUsageFlags usage,
                VkMemoryPropertyFlags properties, VkImageAspectFlags aspect);
    void Create(VkImage image, uint32_t mipLevels, VkFormat format, VkImageAspectFlags aspect);
    void Destroy();

    operator VkImage() { return image; }

    void TransitionLayout(VkImageLayout newLayout);
    void GenerateMipmaps();

private:
    bool CreateImageAndMemory();
    bool CreateView();
};

struct ImageResource {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct ImageDesc {
    VkFormat format;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkSampleCountFlagBits numSamples;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkImageAspectFlags aspect;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint32_t width;
    uint32_t height;
    uint32_t mipLevels = 1;
};

class Image {
public:
    static void Create(const ImageDesc& desc, ImageResource& res);
    static void Destroy(ImageResource& res);
};
