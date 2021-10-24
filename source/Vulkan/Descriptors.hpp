#pragma once

#include <vulkan/vulkan.h>
#include "BufferManager.hpp"

struct TextureDescriptor {
    std::vector<VkDescriptorSet> descriptors;
};

struct BufferDescriptor {
    std::vector<VkDescriptorSet> descriptors;
    std::vector<BufferResource> buffers;
};
