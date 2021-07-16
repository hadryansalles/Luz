#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include "Shader.hpp"

struct GraphicsPipelineDesc {
    std::string                                    name = "Default";
    std::vector<ShaderDesc>                        shaderStages;
    VkVertexInputBindingDescription                bindingDesc{};
    std::vector<VkVertexInputAttributeDescription> attributesDesc;
    VkPipelineRasterizationStateCreateInfo         rasterizer{};
    VkPipelineMultisampleStateCreateInfo           multisampling{};
    VkPipelineDepthStencilStateCreateInfo          depthStencil{};
    VkPipelineColorBlendAttachmentState            colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo            colorBlendState{};
    std::vector<VkDescriptorSetLayoutBinding>      bindings;
};

struct GraphicsPipelineResource {
    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      layout              = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    bool                  dirty               = false;
};

class GraphicsPipeline {
public:
    static void Create(const GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void Destroy(GraphicsPipelineResource& res);
    static void OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
};