#pragma once

#include <vulkan/vulkan.h>

struct TextureDescriptor {
    std::vector<VkDescriptorSet> descriptors;
};

struct BufferDescriptor {
    std::vector<VkDescriptorSet> descriptors;
    std::vector<BufferResource> buffers;
};
