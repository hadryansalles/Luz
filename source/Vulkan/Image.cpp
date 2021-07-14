#include "Luzpch.hpp"

#include "Image.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"

VulkanImage::VulkanImage() 
    : image(VK_NULL_HANDLE)
    , view(VK_NULL_HANDLE)
    , memory(VK_NULL_HANDLE)
{}

void VulkanImage::Create(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                         VkImageTiling tiling, VkSampleCountFlagBits numSamples, VkImageUsageFlags usage, 
                         VkMemoryPropertyFlags properties, VkImageAspectFlags aspect) {
    this->width = width;
    this->height = height;
    this->mipLevels = mipLevels;
    this->format = format;
    this->tiling = tiling;
    this->numSamples = numSamples;
    // sampled defines that the image will be accessed by the shader
    this->usage = usage;
    this->properties = properties;
    this->aspect = aspect;
    // not usable by the GPU, the first transition will discard the texels
    this->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    CreateImageAndMemory();
    CreateView();
}

void VulkanImage::Create(VkImage image, uint32_t mipLevels, VkFormat format, VkImageAspectFlags aspect) {
    this->image = image;
    this->mipLevels = mipLevels;
    this->format = format;
    this->aspect = aspect;

    CreateView();
}

void VulkanImage::Destroy() {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    DEBUG_ASSERT(image  != VK_NULL_HANDLE, "null image at VulkanImage::Destroy");
    DEBUG_ASSERT(view   != VK_NULL_HANDLE, "null view at VulkanImage::Destroy");
    DEBUG_ASSERT(memory != VK_NULL_HANDLE, "null memory at VulkanImage::Destroy");

    vkDestroyImageView(device, view, allocator);
    vkDestroyImage(device, image, allocator);
    vkFreeMemory(device, memory, allocator);
}

void VulkanImage::TransitionLayout(VkImageLayout newLayout) {

}

void VulkanImage::GenerateMipmaps() {

}

bool VulkanImage::CreateImageAndMemory() {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    // tiling defines how the texels lay in memory
    // optimal tiling is implementation dependent for more efficient memory access
    // and linear makes the texels lay in row-major order, possibly with padding on each row
    imageInfo.tiling = tiling;
    // not usable by the GPU, the first transition will discard the texels
    imageInfo.initialLayout = layout;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = numSamples;
    imageInfo.flags = 0;

    auto res = vkCreateImage(device, &imageInfo, allocator, &image);
    if (res != VK_SUCCESS) {
        LOG_ERROR("failed to create image!");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = PhysicalDevice::FindMemoryType(memReq.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &allocInfo, allocator, &memory);
    if (res != VK_SUCCESS) {
        LOG_ERROR("failed to allocate image memory!");
        return false;
    }

    vkBindImageMemory(device, image, memory, 0);
    return true;
}

bool VulkanImage::CreateView() {
    auto device = LogicalDevice::GetVkDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    auto res = vkCreateImageView(device, &viewInfo, Instance::GetAllocator(), &view);
    if (res != VK_SUCCESS) {
        LOG_ERROR("failed to create image view!");
        return false;
    }
    return true;
}

void Image::Create(const ImageDesc& desc, ImageResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = desc.format;
    // tiling defines how the texels lay in memory
    // optimal tiling is implementation dependent for more efficient memory access
    // and linear makes the texels lay in row-major order, possibly with padding on each row
    imageInfo.tiling = desc.tiling;
    // not usable by the GPU, the first transition will discard the texels
    imageInfo.initialLayout = desc.layout;
    imageInfo.usage = desc.usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = desc.numSamples;
    imageInfo.flags = 0;

    auto result = vkCreateImage(device, &imageInfo, allocator, &res.image);
    DEBUG_VK(result, "Failed to create image!");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, res.image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = PhysicalDevice::FindMemoryType(memReq.memoryTypeBits, desc.properties);

    result = vkAllocateMemory(device, &allocInfo, allocator, &res.memory);
    DEBUG_VK(result, "Failed to allocate image memory!");

    vkBindImageMemory(device, res.image, res.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = desc.format;
    viewInfo.subresourceRange.aspectMask = desc.aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device, &viewInfo, allocator, &res.view);
    DEBUG_VK(result, "Failed to create image view!");
}

void Image::Destroy(ImageResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    DEBUG_ASSERT(res.image  != VK_NULL_HANDLE, "Null image at VulkanImage::Destroy");
    DEBUG_ASSERT(res.view   != VK_NULL_HANDLE, "Null view at VulkanImage::Destroy");
    DEBUG_ASSERT(res.memory != VK_NULL_HANDLE, "Null memory at VulkanImage::Destroy");

    vkDestroyImageView(device, res.view, allocator);
    vkDestroyImage(device, res.image, allocator);
    vkFreeMemory(device, res.memory, allocator);
}
