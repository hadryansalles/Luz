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

class GraphicsPipelineManager {
    static inline VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

public:
    static void Create();
    static void Destroy();
    static void CreatePipeline(const GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void DestroyPipeline(GraphicsPipelineResource& res);
    static void OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);

    static inline VkDescriptorPool GetDescriptorPool() { return descriptorPool; }
};
