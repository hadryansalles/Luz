#include "Luzpch.hpp"

#include "BufferManager.hpp"
//#include "LogicalDevice.hpp"
//#include "PhysicalDevice.hpp"
//#include "Instance.hpp"
#include "VulkanLayer.h"

void BufferManager::Create(const BufferDesc& desc, BufferResource2& res) {
    auto device = vkw::ctx().device;
    auto allocator = vkw::ctx().allocator;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;
    bufferInfo.usage = desc.usage;
    // only from the graphics queue
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto result = vkCreateBuffer(device, &bufferInfo, allocator, &res.buffer);
    DEBUG_VK(result, "Failed to create buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, res.buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = vkw::ctx().FindMemoryType(memReq.memoryTypeBits, desc.properties);

    result = vkAllocateMemory(device, &allocInfo, allocator, &res.memory);
    DEBUG_VK(result, "Failed to allocate buffer memory!");

    vkBindBufferMemory(device, res.buffer, res.memory, 0);
}

void BufferManager::Destroy(BufferResource2& res) {
    vkDestroyBuffer(vkw::ctx().device, res.buffer, vkw::ctx().allocator);
    vkFreeMemory(vkw::ctx().device, res.memory, vkw::ctx().allocator);
}

void BufferManager::Copy(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto commandBuffer = vkw::ctx().BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    vkw::ctx().EndSingleTimeCommands(commandBuffer);
}

void BufferManager::Update(BufferResource2& res, void* data, VkDeviceSize size) {
    void* dst;
    vkMapMemory(vkw::ctx().device, res.memory, 0, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(vkw::ctx().device, res.memory);
}

void BufferManager::Update(BufferResource2& res, void* data, VkDeviceSize offset, VkDeviceSize size) {
    void* dst;
    vkMapMemory(vkw::ctx().device, res.memory, offset, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(vkw::ctx().device, res.memory);
}

void BufferManager::CreateStagingBuffer(BufferResource2& res, void* data, VkDeviceSize size) {
    BufferDesc stagingDesc;
    stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingDesc.size = size;
    BufferManager::Create(stagingDesc, res);
    BufferManager::Update(res, data, size);
}

void BufferManager::CreateStorageBuffer(StorageBuffer& buffer, VkDeviceSize size) {
    buffer.dataSize = size;
    u64 numFrames = vkw::ctx().swapChainImages.size();
    BufferDesc bufferDesc;
    VkDeviceSize minOffset = vkw::ctx().physicalProperties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize sizeRemain = size % minOffset;
    buffer.sectionSize = size;
    if (sizeRemain > 0) {
        buffer.sectionSize += minOffset - sizeRemain;
    }
    bufferDesc.size = buffer.sectionSize * numFrames;
    bufferDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    BufferManager::Create(bufferDesc, buffer.resource);
}

void BufferManager::DestroyStorageBuffer(StorageBuffer& buffer) {
    Destroy(buffer.resource);
}

void BufferManager::UpdateStorage(StorageBuffer& buffer, int numFrame, void* data) {
    BufferManager::Update(buffer.resource, data, numFrame * buffer.sectionSize, buffer.dataSize);
}

VkDeviceAddress BufferManager::GetAddress(BufferResource2& res) {
    VkBufferDeviceAddressInfo info{};
    info.buffer = res.buffer;
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    return vkGetBufferDeviceAddress(vkw::ctx().device, &info);
}