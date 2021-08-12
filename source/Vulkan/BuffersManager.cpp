#include "Luzpch.hpp"

#include "BuffersManager.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"

void BuffersManager::Create(const BufferDesc& desc, BufferResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

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
    allocInfo.memoryTypeIndex = PhysicalDevice::FindMemoryType(memReq.memoryTypeBits, desc.properties);

    result = vkAllocateMemory(device, &allocInfo, allocator, &res.memory);
    DEBUG_VK(result, "Failed to allocate buffer memory!");

    vkBindBufferMemory(device, res.buffer, res.memory, 0);
}

void BuffersManager::Destroy(BufferResource& res) {
    vkDestroyBuffer(LogicalDevice::GetVkDevice(), res.buffer, Instance::GetAllocator());
    vkFreeMemory(LogicalDevice::GetVkDevice(), res.memory, Instance::GetAllocator());
}

void BuffersManager::Copy(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto commandBuffer = LogicalDevice::BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    LogicalDevice::EndSingleTimeCommands(commandBuffer);
}

void BuffersManager::Update(BufferResource& res, void* data, VkDeviceSize size) {
    void* dst;
    vkMapMemory(LogicalDevice::GetVkDevice(), res.memory, 0, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(LogicalDevice::GetVkDevice(), res.memory);
}

void BuffersManager::CreateStaged(const BufferDesc& desc, BufferResource& res, void* data) {
    BufferResource staging;
    BuffersManager::Create(desc, res);
    CreateStagingBuffer(staging, data, desc.size);
    BuffersManager::Copy(staging.buffer, res.buffer, desc.size);
    BuffersManager::Destroy(staging);
}

void BuffersManager::CreateStagingBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc stagingDesc;
    stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingDesc.size = size;
    BuffersManager::Create(stagingDesc, res);
    BuffersManager::Update(res, data, size);
}

void BuffersManager::CreateIndexBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc desc;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.size = size;
    BuffersManager::CreateStaged(desc, res, data);
}

void BuffersManager::CreateVertexBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc desc;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.size = size;
    BuffersManager::CreateStaged(desc, res, data);
}

