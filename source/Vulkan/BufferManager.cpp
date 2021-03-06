#include "Luzpch.hpp"

#include "BufferManager.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "SwapChain.hpp"

void BufferManager::Create(const BufferDesc& desc, BufferResource& res) {
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

void BufferManager::Destroy(BufferResource& res) {
    vkDestroyBuffer(LogicalDevice::GetVkDevice(), res.buffer, Instance::GetAllocator());
    vkFreeMemory(LogicalDevice::GetVkDevice(), res.memory, Instance::GetAllocator());
}

void BufferManager::Copy(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto commandBuffer = LogicalDevice::BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    LogicalDevice::EndSingleTimeCommands(commandBuffer);
}

void BufferManager::Update(BufferResource& res, void* data, VkDeviceSize size) {
    void* dst;
    vkMapMemory(LogicalDevice::GetVkDevice(), res.memory, 0, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(LogicalDevice::GetVkDevice(), res.memory);
}

void BufferManager::Update(BufferResource& res, void* data, VkDeviceSize offset, VkDeviceSize size) {
    void* dst;
    vkMapMemory(LogicalDevice::GetVkDevice(), res.memory, offset, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(LogicalDevice::GetVkDevice(), res.memory);
}

void BufferManager::CreateStaged(const BufferDesc& desc, BufferResource& res, void* data) {
    static int i = 0;
    i++;
    BufferResource staging;
    BufferManager::Create(desc, res);
    CreateStagingBuffer(staging, data, desc.size);
    BufferManager::Copy(staging.buffer, res.buffer, desc.size);
    BufferManager::Destroy(staging);
}

void BufferManager::CreateStagingBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc stagingDesc;
    stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingDesc.size = size;
    BufferManager::Create(stagingDesc, res);
    BufferManager::Update(res, data, size);
}

void BufferManager::CreateIndexBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc desc;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.size = size;
    BufferManager::CreateStaged(desc, res, data);
}

void BufferManager::CreateVertexBuffer(BufferResource& res, void* data, VkDeviceSize size) {
    BufferDesc desc;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.size = size;
    BufferManager::CreateStaged(desc, res, data);
}

void BufferManager::CreateUniformBuffer(UniformBuffer& uniform, VkDeviceSize size) {
    uniform.dataSize = size;
    u64 numFrames = SwapChain::GetNumFrames();
    BufferDesc bufferDesc;
    VkDeviceSize minOffset = PhysicalDevice::GetProperties().limits.minUniformBufferOffsetAlignment;
    uniform.sectionSize = ALIGN_AS(size, minOffset);
    bufferDesc.size = uniform.sectionSize * numFrames;
    bufferDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    BufferManager::Create(bufferDesc, uniform.resource);
    SetDirtyUniform(uniform);
}

void BufferManager::DestroyUniformBuffer(UniformBuffer& uniform) {
    Destroy(uniform.resource);
}

void BufferManager::SetDirtyUniform(UniformBuffer& uniform) {
    uniform.dirty = std::vector<bool>(SwapChain::GetNumFrames(), true);
}

void BufferManager::UpdateUniformIfDirty(UniformBuffer& uniform, int numFrame, void* data) {
    DEBUG_ASSERT(numFrame < uniform.dirty.size(), "Invalid frame number!");
    if(uniform.dirty[numFrame]) {
        BufferManager::Update(uniform.resource, data, numFrame * uniform.sectionSize, uniform.dataSize);
        uniform.dirty[numFrame] = false;
    }
}

void BufferManager::CreateStorageBuffer(StorageBuffer& buffer, VkDeviceSize size) {
    buffer.dataSize = size;
    u64 numFrames = SwapChain::GetNumFrames();
    BufferDesc bufferDesc;
    VkDeviceSize minOffset = PhysicalDevice::GetProperties().limits.minStorageBufferOffsetAlignment;
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

VkDeviceAddress BufferManager::GetAddress(BufferResource& res) {
    VkBufferDeviceAddressInfo info{};
    info.buffer = res.buffer;
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    return vkGetBufferDeviceAddress(LogicalDevice::GetVkDevice(), &info);
}