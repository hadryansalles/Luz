#pragma once

#include <vulkan/vulkan.h>
#include "VulkanLayer.h"

struct BufferDesc {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
};

struct BufferResource2 {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct StorageBuffer {
    BufferResource2 resource;
    VkDeviceSize dataSize;
    VkDeviceSize sectionSize;
};

class BufferManager {
public:
    static void Create(const BufferDesc& desc, BufferResource2& res);
    static void CreateStaged(const BufferDesc& desc, BufferResource2& res, void* data);
    static void Destroy(BufferResource2& res);

    static void Copy(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    static void Update(BufferResource2& res, void* data, VkDeviceSize size);
    static void Update(BufferResource2& res, void* data, VkDeviceSize offet, VkDeviceSize size);

    static void CreateStagingBuffer(BufferResource2& res, void* data, VkDeviceSize size);
    static void CreateIndexBuffer(BufferResource2& res, void* data, VkDeviceSize size);
    static void CreateVertexBuffer(BufferResource2& res, void* data, VkDeviceSize size);

    static void CreateStorageBuffer(StorageBuffer& uniform, VkDeviceSize dataSize);
    static void DestroyStorageBuffer(StorageBuffer& uniform);
    static void UpdateStorage(StorageBuffer& uniform, int numFrame, void* data);

    static VkDeviceAddress GetAddress(BufferResource2& res);
};