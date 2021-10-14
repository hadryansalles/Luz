#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include "TextureManager.hpp"
#include "Descriptors.hpp"
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
    std::vector<VkDescriptorSetLayoutBinding>      bindings;
};

struct GraphicsPipelineResource {
    VkPipeline            pipeline                    = VK_NULL_HANDLE;
    VkPipelineLayout      layout                      = VK_NULL_HANDLE;
    VkDescriptorSetLayout sceneDescriptorSetLayout    = VK_NULL_HANDLE;
    VkDescriptorSetLayout meshDescriptorSetLayout     = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescriptorSetLayout  = VK_NULL_HANDLE;
    bool                  dirty                       = false;
};

class GraphicsPipelineManager {
    static inline VkDescriptorPool sceneDescriptorPool    = VK_NULL_HANDLE;
    static inline VkDescriptorPool meshDescriptorPool     = VK_NULL_HANDLE;
    static inline VkDescriptorPool materialDescriptorPool = VK_NULL_HANDLE;
    static inline VkDescriptorPool textureDescriptorPool  = VK_NULL_HANDLE;
    static inline VkDescriptorPool imguiDescriptorPool    = VK_NULL_HANDLE;

public:
    static void Create();
    static void Destroy();
    static void CreatePipeline(const GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);
    static void DestroyPipeline(GraphicsPipelineResource& res);
    static void OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res);

    static BufferDescriptor  CreateSceneDescriptor(uint32_t size);
    static BufferDescriptor  CreateMeshDescriptor(uint32_t size);
    static BufferDescriptor  CreateMaterialDescriptor(uint32_t size);
    static TextureDescriptor CreateTextureDescriptor(VkDescriptorSetLayout setLayout);

    static void UpdateBufferDescriptor(BufferDescriptor& descriptor, void* data, uint32_t size);
    static void UpdateTextureDescriptor(TextureDescriptor& descriptor, TextureResource* texture);

    static inline VkDescriptorPool GetImguiDescriptorPool() { return imguiDescriptorPool; }
};
