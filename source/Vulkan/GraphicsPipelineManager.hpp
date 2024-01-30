#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include "Shader.hpp"
#include "VulkanLayer.h"

struct GraphicsPipelineDesc {
    std::string                                    name = "Default";
    std::vector<ShaderDesc>                        shaderStages;
    VkVertexInputBindingDescription                bindingDesc{};
    std::vector<VkVertexInputAttributeDescription> attributesDesc;
    VkPipelineRasterizationStateCreateInfo         rasterizer{};
    VkPipelineMultisampleStateCreateInfo           multisampling{};
    VkPipelineDepthStencilStateCreateInfo          depthStencil{};
    std::vector<VkFormat> colorFormats;
    bool useDepthAttachment = false;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
};

struct GraphicsPipelineResource {
    VkPipeline            pipeline = VK_NULL_HANDLE;
    VkPipelineLayout      layout   = VK_NULL_HANDLE;
    bool                  dirty    = false;
    std::vector<ShaderResource> shaderResources;
};

class GraphicsPipelineManager {
    // static inline VkDescriptorPool imguiDescriptorPool   = VK_NULL_HANDLE;

    // // bindless resources
    // static inline VkDescriptorSet bindlessDescriptorSet          = VK_NULL_HANDLE;
    // static inline VkDescriptorPool bindlessDescriptorPool        = VK_NULL_HANDLE;
    // static inline VkDescriptorSetLayout bindlessDescriptorLayout = VK_NULL_HANDLE;

public:
    static void Create();
    static void Destroy();
    static void CreateDefaultDesc(GraphicsPipelineDesc& desc);
    static void CreatePipeline(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void DestroyPipeline(GraphicsPipelineResource& res);
    static void DestroyShaders(GraphicsPipelineResource& res);
    static void ReloadShaders(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);

    static inline VkDescriptorPool GetImguiDescriptorPool()    { return vkw::ctx().imguiDescriptorPool;   }

    // bindless resources
    static inline VkDescriptorSet& GetBindlessDescriptorSet() { return vkw::ctx().bindlessDescriptorSet; }

    static inline constexpr int TEXTURES_BINDING = 0;
    static inline constexpr int STORAGE_BINDING = 1;
    static inline constexpr int ACCELERATION_STRUCTURE_BINDING = 2;
    static inline constexpr int IMAGE_ATTACHMENT_BINDING = 3;
};
