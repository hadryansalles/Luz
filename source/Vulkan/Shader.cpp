#include "Luzpch.hpp"

#include "Shader.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include "FileManager.hpp"

#include <spirv_reflect.h>
#include <cstdlib>
#include <filesystem>

void Shader::Create(const ShaderDesc& desc, ShaderResource& res) {
    // LOG_INFO("SPIRV compiler: %s", );
    char compile_string[1024];
    char inpath[256];
    char outpath[256];
    std::string cwd = std::filesystem::current_path().string();
    sprintf(inpath, "%s/source/Shaders/%s", cwd.c_str(), desc.path.c_str());
    sprintf(outpath, "%s/bin/%s.spv", cwd.c_str(), std::filesystem::path(desc.path).filename().string().c_str());
    sprintf(compile_string, "%s -V %s -o %s --target-env spirv1.4", GLSL_VALIDATOR, inpath, outpath);
    DEBUG_TRACE("[ShaderCompiler] Command: {}", compile_string);
    DEBUG_TRACE("[ShaderCompiler] Output:");
    if(system(compile_string)) {
        LOG_ERROR("[ShaderCompiler] Error!");
    }

    std::vector<char> shaderBytes = FileManager::ReadRawBytes(outpath);

    auto device = vkw::ctx().device;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderBytes.size();
    createInfo.pCode = (const uint32_t*)(shaderBytes.data());
    auto result = vkCreateShaderModule(device, &createInfo, vkw::ctx().allocator, &res.shaderModule);
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
    vkDestroyShaderModule(vkw::ctx().device, res.shaderModule, vkw::ctx().allocator);
}
