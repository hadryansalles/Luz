#pragma once

#include <vulkan/vulkan.h>

struct BufferDesc {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
};

struct BufferResource {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

class BuffersManager {
public:
    static void Create(const BufferDesc& desc, BufferResource& res);
    static void CreateStaged(const BufferDesc& desc, BufferResource& res, void* data);
    static void Destroy(BufferResource& res);

    static void Copy(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    static void Update(BufferResource& res, void* data, VkDeviceSize size);

    static void CreateStagingBuffer(BufferResource& res, void* data, VkDeviceSize size);
    static void CreateIndexBuffer(BufferResource& res, void* data, VkDeviceSize size);
    static void CreateVertexBuffer(BufferResource& res, void* data, VkDeviceSize size);
};
