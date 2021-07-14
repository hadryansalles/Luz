#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct ShaderDesc {
    std::vector<char> shaderBytes;
    VkShaderStageFlagBits stageBit;
};

struct ShaderResource {
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stageCreateInfo{};
};

class Shader {
public:
    static void Create(const ShaderDesc& desc, ShaderResource& res);
    static void Destroy(ShaderResource& res);
};