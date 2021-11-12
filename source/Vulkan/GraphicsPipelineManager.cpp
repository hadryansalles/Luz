#include "Luzpch.hpp"

#include "GraphicsPipelineManager.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "SwapChain.hpp"
#include "Instance.hpp"
#include "VulkanUtils.hpp"
#include "UnlitGraphicsPipeline.hpp"


void GraphicsPipelineManager::Create() {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();
    auto numFrames = SwapChain::GetNumFrames();

    VkDescriptorPoolSize imguiPoolSizes[]    = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} };

    VkDescriptorPoolSize bufferPoolSizes[]   = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024} };
    VkDescriptorPoolCreateInfo bufferPoolInfo{};
    bufferPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    bufferPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    bufferPoolInfo.maxSets = (uint32_t)(1024);
    bufferPoolInfo.poolSizeCount = 1;
    bufferPoolInfo.pPoolSizes = bufferPoolSizes;

    VkDescriptorPool bufferDescriptorPool;
    auto result = vkCreateDescriptorPool(device, &bufferPoolInfo, allocator, &bufferDescriptorPool);
    bufferDescriptorPools.push_back(bufferDescriptorPool);
    DEBUG_VK(result, "Failed to create buffer descriptor pool!");

    VkDescriptorPoolCreateInfo imguiPoolInfo{};
    imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imguiPoolInfo.maxSets = (uint32_t)(1024);
    imguiPoolInfo.poolSizeCount = sizeof(imguiPoolSizes)/sizeof(VkDescriptorPoolSize);
    imguiPoolInfo.pPoolSizes = imguiPoolSizes;

    result = vkCreateDescriptorPool(device, &imguiPoolInfo, allocator, &imguiDescriptorPool);
    DEBUG_VK(result, "Failed to create imgui descriptor pool!");

    // create bindless resources
    {
        const u32 MAX_UNIFORMS = PhysicalDevice::GetProperties().limits.maxPerStageDescriptorUniformBuffers-10;
        const u32 MAX_STORAGE = PhysicalDevice::GetProperties().limits.maxPerStageDescriptorStorageBuffers;
        const u32 MAX_TEXTURES = PhysicalDevice::GetProperties().limits.maxPerStageDescriptorSampledImages-10;

        // create descriptor set pool for bindless resources
        std::vector<VkDescriptorPoolSize> bindlessPoolSizes = { 
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_UNIFORMS},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE}
        };

        VkDescriptorPoolCreateInfo bindlessPoolInfo{};
        bindlessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        bindlessPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        bindlessPoolInfo.maxSets = 1;
        bindlessPoolInfo.poolSizeCount = bindlessPoolSizes.size();
        bindlessPoolInfo.pPoolSizes = bindlessPoolSizes.data();

        result = vkCreateDescriptorPool(device, &bindlessPoolInfo, allocator, &bindlessDescriptorPool);
        DEBUG_VK(result, "Failed to create bindless descriptor pool!");

        // create descriptor set layout for bindless resources
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> bindingFlags;

        VkDescriptorSetLayoutBinding texturesBinding{};
        texturesBinding.binding = TEXTURES_BINDING;
        texturesBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texturesBinding.descriptorCount = MAX_TEXTURES;
        texturesBinding.stageFlags = VK_SHADER_STAGE_ALL;
        bindings.push_back(texturesBinding);
        bindingFlags.push_back({ VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT });

        VkDescriptorSetLayoutBinding storageBuffersBinding{};
        storageBuffersBinding.binding = STORAGE_BINDING;
        storageBuffersBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storageBuffersBinding.descriptorCount = MAX_STORAGE;
        storageBuffersBinding.stageFlags = VK_SHADER_STAGE_ALL;
        bindings.push_back(storageBuffersBinding);
        bindingFlags.push_back({ VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT });

        VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
        setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        setLayoutBindingFlags.bindingCount = bindingFlags.size();
        setLayoutBindingFlags.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = bindings.size();
        descriptorLayoutInfo.pBindings = bindings.data();
        descriptorLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descriptorLayoutInfo.pNext = &setLayoutBindingFlags;

        result = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &bindlessDescriptorLayout);
        DEBUG_VK(result, "Failed to create bindless descriptor set layout!");

        // create descriptor set for bindless resources
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = bindlessDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &bindlessDescriptorLayout;

        result = vkAllocateDescriptorSets(device, &allocInfo, &bindlessDescriptorSet);
        DEBUG_VK(result, "Failed to allocate bindless descriptor set!");
    }
}

void GraphicsPipelineManager::Destroy() {
    for (int i = 0; i < bufferDescriptorPools.size(); i++) {
        vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), bufferDescriptorPools[i], Instance::GetAllocator());
    }
    bufferDescriptorPools.clear();
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), imguiDescriptorPool, Instance::GetAllocator());

    // bindless resources
    {
        vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), bindlessDescriptorPool, Instance::GetAllocator());
        vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), bindlessDescriptorLayout, Instance::GetAllocator());
        bindlessDescriptorSet = VK_NULL_HANDLE;
        bindlessDescriptorPool = VK_NULL_HANDLE;
        bindlessDescriptorLayout = VK_NULL_HANDLE;
    }
}

void GraphicsPipelineManager::CreatePipeline(const GraphicsPipelineDesc& desc, GraphicsPipelineResource& res) {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    std::vector<ShaderResource> shaderResources(desc.shaderStages.size());
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(desc.shaderStages.size());

    for (int i = 0; i < shaderResources.size(); i++) {
        Shader::Create(desc.shaderStages[i], shaderResources[i]);
        shaderStages[i] = shaderResources[i].stageCreateInfo;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)(desc.attributesDesc.size());
    // these points to an array of structs that describe how to load the vertex data
    vertexInputInfo.pVertexBindingDescriptions = &desc.bindingDesc;
    vertexInputInfo.pVertexAttributeDescriptions = desc.attributesDesc.data();

    // define the type of input of our pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // with this parameter true we can break up lines and triangles in _STRIP topology modes
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float> (SwapChain::GetExtent().width);
    viewport.height = static_cast<float> (SwapChain::GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = SwapChain::GetExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    std::vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(bindlessDescriptorLayout);

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = 128;
    pushConstant.stageFlags = VK_SHADER_STAGE_ALL;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    auto vkRes = vkCreatePipelineLayout(device, &pipelineLayoutInfo, allocator, &res.layout);
    DEBUG_VK(vkRes, "Failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &desc.rasterizer;
    pipelineInfo.pMultisampleState = &desc.multisampling;
    pipelineInfo.pDepthStencilState = &desc.depthStencil;
    pipelineInfo.pColorBlendState = &desc.colorBlendState;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = res.layout;
    pipelineInfo.renderPass = SwapChain::GetRenderPass();
    pipelineInfo.subpass = 0;
    // if we were creating this pipeline by deriving it from another
    // we should specify here
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    vkRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, allocator, &res.pipeline);
    DEBUG_VK(vkRes, "Failed to create graphics pipeline!");

    for (int i = 0; i < shaderResources.size(); i++) {
        Shader::Destroy(shaderResources[i]);
    }

    res.dirty = false;
}

void GraphicsPipelineManager::DestroyPipeline(GraphicsPipelineResource& res) {
    vkDestroyPipeline(LogicalDevice::GetVkDevice(), res.pipeline, Instance::GetAllocator());
    vkDestroyPipelineLayout(LogicalDevice::GetVkDevice(), res.layout, Instance::GetAllocator());
    for (int i = 0; i < res.descriptorSetLayouts.size(); i++) {
        vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.descriptorSetLayouts[i], Instance::GetAllocator());
    }
}

void GraphicsPipelineManager::OnImgui(GraphicsPipelineDesc& desc, GraphicsPipelineResource& res) {
    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    std::string name = desc.name + " Graphics Pipeline";
    if (ImGui::CollapsingHeader(name.c_str())) {
        // polygon mode
        {
            if (PhysicalDevice::GetFeatures().fillModeNonSolid) {
                ImGui::Text("Polygon Mode");
                ImGui::SameLine(totalWidth*3.0/5.0f);
                ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
                ImGui::PushID("polygonMode");
                if (ImGui::BeginCombo("", LUZ_VkPolygonModeStr(desc.rasterizer.polygonMode))) {
                    for (auto mode : LUZ_VkPolygonModes()) {
                        bool selected = mode == desc.rasterizer.polygonMode;
                        if (ImGui::Selectable(LUZ_VkPolygonModeStr(mode), selected) && !selected) {
                            desc.rasterizer.polygonMode = mode;
                            res.dirty = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
        }
        // line width
        {
            if (PhysicalDevice::GetFeatures().wideLines) {
                ImGui::Text("Line width");
                ImGui::SameLine(totalWidth * 3.0 / 5.0f);
                ImGui::SetNextItemWidth(totalWidth * 2.0 / 5.0f);
                ImGui::PushID("lineWidth");
                if (ImGui::DragFloat("", &desc.rasterizer.lineWidth, 0.01f, 0.01f, 10.0f)) {
                    res.dirty = true;
                }
                ImGui::PopID();
            }
        }
        // cull mode
        {
            ImGui::Text("Cull Mode");
            ImGui::SameLine(totalWidth*3.0/5.0f);
            ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
            ImGui::PushID("cullMode");
            if (ImGui::BeginCombo("", LUZ_VkCullModeStr(desc.rasterizer.cullMode))) {
                for (auto mode : LUZ_VkCullModes()) {
                    bool selected = mode == desc.rasterizer.cullMode;
                    if (ImGui::Selectable(LUZ_VkCullModeStr(mode), selected) && !selected) {
                        desc.rasterizer.cullMode = mode;
                        res.dirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        // sample shading
        {
            if (PhysicalDevice::GetFeatures().sampleRateShading) {
                ImGui::Text("Sample Shading");
                ImGui::SameLine(totalWidth*3.0f/5.0f);
                ImGui::PushID("sampleShading");
                if (ImGui::Checkbox("", (bool*)&desc.multisampling.sampleShadingEnable)) {
                    res.dirty = true;
                }
                ImGui::PopID();
                if(desc.multisampling.sampleShadingEnable) {
                    ImGui::Text("Min Sample Shading");
                    ImGui::SameLine(totalWidth * 3.0f / 5.0f);
                    ImGui::SetNextItemWidth(totalWidth * 2.0f / 5.0f);
                    ImGui::PushID("minSampleShading");
                    if (ImGui::DragFloat("", &desc.multisampling.minSampleShading, 0.0001f, 0.0f, 1.0f)) {
                        res.dirty = true;
                    }
                    ImGui::PopID();
                }
            }
        }
        // depth clamp
        {
            if (PhysicalDevice::GetFeatures().depthClamp) {
                ImGui::Text("Depth Clamp");
                ImGui::SameLine(totalWidth * 3.0f / 5.0f);
                ImGui::PushID("depthClamp");
                if (ImGui::Checkbox("", (bool*)&desc.rasterizer.depthClampEnable)) {
                    res.dirty = true;
                }
                ImGui::PopID();
            }
        }
    }
}

void GraphicsPipelineManager::WriteStorage(StorageBuffer& uniform, int index) {
    int numFrames = SwapChain::GetNumFrames();
    std::vector<VkDescriptorBufferInfo> bufferInfos(numFrames);
    std::vector<VkWriteDescriptorSet> writes(numFrames);
    for (int i = 0; i < numFrames; i++) {
        bufferInfos[i].buffer = uniform.resource.buffer;
        bufferInfos[i].offset = i*uniform.sectionSize;
        bufferInfos[i].range = uniform.dataSize;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = GraphicsPipelineManager::STORAGE_BINDING;
        writes[i].dstArrayElement = numFrames*index + i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }
    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), numFrames, writes.data(), 0, nullptr);
}