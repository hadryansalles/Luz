#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include "BufferManager.hpp"
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
};

struct GraphicsPipelineResource {
    VkPipeline            pipeline                    = VK_NULL_HANDLE;
    VkPipelineLayout      layout                      = VK_NULL_HANDLE;
    bool                  dirty                       = false;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};

struct BindlessPushConstant {
    int frameID;
    int numFrames;
    int textureID;
    int modelID;
};

class GraphicsPipelineManager {
    static inline VkDescriptorPool imguiDescriptorPool   = VK_NULL_HANDLE;

    static inline int bufferDescriptors = 0;
    static inline std::vector<VkDescriptorPool> bufferDescriptorPools;

    // bindless resources
    static inline VkDescriptorSet bindlessDescriptorSet          = VK_NULL_HANDLE;
    static inline VkDescriptorPool bindlessDescriptorPool        = VK_NULL_HANDLE;
    static inline VkDescriptorSetLayout bindlessDescriptorLayout = VK_NULL_HANDLE;

public:
    static void Create();
    static void Destroy();
    static void CreatePipeline(const GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void DestroyPipeline(GraphicsPipelineResource& res);
    static void OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);

    static void WriteStorage(StorageBuffer& uniform, int index);

    static inline VkDescriptorPool GetImguiDescriptorPool()    { return imguiDescriptorPool;   }

    // bindless resources
    static inline VkDescriptorSet& GetBindlessDescriptorSet() { return bindlessDescriptorSet; }

    static inline constexpr int TEXTURES_BINDING = 0;
    static inline constexpr int STORAGE_BINDING = 1;
};
