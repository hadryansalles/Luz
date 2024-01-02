#include "Luzpch.hpp"

#include "ImageManager.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"

#include <vulkan/vulkan.hpp>

void ImageManager::Create(const ImageDesc& desc, ImageResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    res.width = desc.width;
    res.height = desc.height;

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
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    if (desc.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = desc.layout;
        barrier.image = res.image;
        barrier.subresourceRange = viewInfo.subresourceRange;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        VkCommandBuffer commandBuffer = LogicalDevice::BeginSingleTimeCommands();
        vkCmdPipelineBarrier(commandBuffer, stage, stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        LogicalDevice::EndSingleTimeCommands(commandBuffer);
    }
}

void ImageManager::Create(const ImageDesc& desc, ImageResource& res, BufferResource& buffer) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    res.width = desc.width;
    res.height = desc.height;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = desc.format;
    imageInfo.tiling = desc.tiling;
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

    VkCommandBuffer commandBuffer = LogicalDevice::BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    // if we were transferring queue family ownership
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = res.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.levelCount = desc.mipLevels;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    LogicalDevice::EndSingleTimeCommands(commandBuffer);

    commandBuffer = LogicalDevice::BeginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { desc.width, desc.height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer.buffer, res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    LogicalDevice::EndSingleTimeCommands(commandBuffer);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(PhysicalDevice::GetVkPhysicalDevice(), desc.format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        LOG_ERROR("texture image format does not support linear blitting!");
    }

    commandBuffer = LogicalDevice::BeginSingleTimeCommands();
    
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = desc.width;
    int32_t mipHeight = desc.height;

    for (uint32_t i = 1; i < desc.mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer, res.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = desc.mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    LogicalDevice::EndSingleTimeCommands(commandBuffer);

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

void ImageManager::Create(void* data, u32 width, u32 height, u16 channels, u32 mipLevels, ImageResource& res) {
    ImageDesc imageDesc{};
    imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.mipLevels = mipLevels;
    imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    imageDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageDesc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    imageDesc.size = (u64) width * height * channels;

    BufferResource staging;
    BufferManager::CreateStagingBuffer(staging, data, imageDesc.size);
    Create(imageDesc, res, staging);
    BufferManager::Destroy(staging);
}

void ImageManager::Destroy(ImageResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    DEBUG_ASSERT(res.image  != VK_NULL_HANDLE, "Null image at VulkanImage::Destroy");
    DEBUG_ASSERT(res.view   != VK_NULL_HANDLE, "Null view at VulkanImage::Destroy");
    DEBUG_ASSERT(res.memory != VK_NULL_HANDLE, "Null memory at VulkanImage::Destroy");

    vkDestroyImageView(device, res.view, allocator);
    vkDestroyImage(device, res.image, allocator);
    vkFreeMemory(device, res.memory, allocator);
}

void ImageManager::InsertBarrier (
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkImageSubresourceRange subresourceRange
) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ImageManager::BarrierColorUndefinedToAttachment(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = VK_REMAINING_MIP_LEVELS;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    ImageManager::InsertBarrier(
        commandBuffer,
        image,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        colorRange
    );
}

void ImageManager::BarrierDepthUndefinedToAttachment(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.baseMipLevel = 0;
    depthRange.levelCount = VK_REMAINING_MIP_LEVELS;
    depthRange.baseArrayLayer = 0;
    depthRange.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    ImageManager::InsertBarrier (
        commandBuffer,
        image,
        0,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        depthRange
    );

}

void ImageManager::BarrierColorAttachmentToPresent(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = VK_REMAINING_MIP_LEVELS;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    ImageManager::InsertBarrier(
        commandBuffer,
        image,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        colorRange
    );
}

void ImageManager::BarrierColorAttachmentToRead(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = VK_REMAINING_MIP_LEVELS;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    ImageManager::InsertBarrier(
        commandBuffer,
        image,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        colorRange
    );
}

void ImageManager::BarrierDepthAttachmentToRead(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = VK_REMAINING_MIP_LEVELS;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    ImageManager::InsertBarrier(
        commandBuffer,
        image,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        colorRange
    );VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
}VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
