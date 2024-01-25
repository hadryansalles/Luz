#include "Luzpch.hpp"

#include "ImageManager.hpp"
#include "VulkanLayer.h"

#include <vulkan/vulkan.hpp>

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

    srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

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
    );
}
