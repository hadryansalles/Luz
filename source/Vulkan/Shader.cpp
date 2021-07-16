#include "Luzpch.hpp"

#include "Shader.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include <spirv_reflect.h>

void Shader::Create(const ShaderDesc& desc, ShaderResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = desc.shaderBytes.size();
    createInfo.pCode = (const uint32_t*)(desc.shaderBytes.data());
    auto result = vkCreateShaderModule(device, &createInfo, Instance::GetAllocator(), &res.shaderModule);
    DEBUG_VK(result, "Failed to create shader module!");

    res.stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    res.stageCreateInfo.stage = desc.stageBit;
    res.stageCreateInfo.module = res.shaderModule;
    // function to invoke inside the shader module
    // it's possible to combine multiple shaders into a single module
    // using different entry points
    res.stageCreateInfo.pName = "main";
    // this allow us to specify values for shader constants
    res.stageCreateInfo.pSpecializationInfo = nullptr;
}

void Shader::Destroy(ShaderResource& res) {
    vkDestroyShaderModule(LogicalDevice::GetVkDevice(), res.shaderModule, Instance::GetAllocator());
}
