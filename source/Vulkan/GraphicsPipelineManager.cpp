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

    VkDescriptorPoolSize scenePoolSizes[]    = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024} };
    VkDescriptorPoolSize meshPoolSizes[]     = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024} };
    VkDescriptorPoolSize materialPoolSizes[] = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024} };
    VkDescriptorPoolSize texturePoolSizes[]  = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024} };
    VkDescriptorPoolSize imguiPoolSizes[]    = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} };

    VkDescriptorPoolCreateInfo scenePoolInfo{};
    scenePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    scenePoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    scenePoolInfo.maxSets = (uint32_t)(1024);
    scenePoolInfo.poolSizeCount = 1;
    scenePoolInfo.pPoolSizes = scenePoolSizes;

    auto result = vkCreateDescriptorPool(device, &scenePoolInfo, allocator, &sceneDescriptorPool);
    DEBUG_VK(result, "Failed to create scene descriptor pool!");

    VkDescriptorPoolCreateInfo meshPoolInfo{};
    meshPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    meshPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    meshPoolInfo.maxSets = (uint32_t)(1024);
    meshPoolInfo.poolSizeCount = 1;
    meshPoolInfo.pPoolSizes = meshPoolSizes;

    result = vkCreateDescriptorPool(device, &meshPoolInfo, allocator, &meshDescriptorPool);
    DEBUG_VK(result, "Failed to create mesh descriptor pool!");

    VkDescriptorPoolCreateInfo materialPoolInfo{};
    materialPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    materialPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    materialPoolInfo.maxSets = (uint32_t)(1024);
    materialPoolInfo.poolSizeCount = 1;
    materialPoolInfo.pPoolSizes = materialPoolSizes;

    result = vkCreateDescriptorPool(device, &materialPoolInfo, allocator, &materialDescriptorPool);
    DEBUG_VK(result, "Failed to create material descriptor pool!");

    VkDescriptorPoolCreateInfo texturePoolInfo{};
    texturePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    texturePoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    texturePoolInfo.maxSets = (uint32_t)(1024);
    texturePoolInfo.poolSizeCount = 1;
    texturePoolInfo.pPoolSizes = texturePoolSizes;

    result = vkCreateDescriptorPool(device, &texturePoolInfo, allocator, &textureDescriptorPool);
    DEBUG_VK(result, "Failed to create texture descriptor pool!");

    VkDescriptorPoolCreateInfo imguiPoolInfo{};
    imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imguiPoolInfo.maxSets = (uint32_t)(1024);
    imguiPoolInfo.poolSizeCount = sizeof(imguiPoolSizes)/sizeof(VkDescriptorPoolSize);
    imguiPoolInfo.pPoolSizes = imguiPoolSizes;

    result = vkCreateDescriptorPool(device, &imguiPoolInfo, allocator, &imguiDescriptorPool);
    DEBUG_VK(result, "Failed to create imgui descriptor pool!");
}

void GraphicsPipelineManager::Destroy() {
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), sceneDescriptorPool, Instance::GetAllocator());
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), meshDescriptorPool, Instance::GetAllocator());
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), materialDescriptorPool, Instance::GetAllocator());
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), textureDescriptorPool, Instance::GetAllocator());
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), imguiDescriptorPool, Instance::GetAllocator());
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

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &desc.bindings[0];

    auto vkRes = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &res.sceneDescriptorSetLayout);
    DEBUG_VK(vkRes, "Failed to create scene descriptor set layout!");

    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &desc.bindings[1];

    vkRes = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &res.meshDescriptorSetLayout);
    DEBUG_VK(vkRes, "Failed to create model descriptor set layout!");

    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &desc.bindings[2];

    vkRes = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &res.materialDescriptorSetLayout);
    DEBUG_VK(vkRes, "Failed to create material descriptor set layout!");

    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &desc.bindings[3];

    vkRes = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &res.textureDescriptorSetLayout);
    DEBUG_VK(vkRes, "Failed to create texture descriptor set layout!");

    VkDescriptorSetLayout setLayouts[] = { res.sceneDescriptorSetLayout, res.meshDescriptorSetLayout, res.materialDescriptorSetLayout, res.textureDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = sizeof(setLayouts)/sizeof(VkDescriptorSetLayout);
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    vkRes = vkCreatePipelineLayout(device, &pipelineLayoutInfo, allocator, &res.layout);
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
    vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.sceneDescriptorSetLayout, Instance::GetAllocator());
    vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.meshDescriptorSetLayout, Instance::GetAllocator());
    vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.materialDescriptorSetLayout, Instance::GetAllocator());
    vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.textureDescriptorSetLayout, Instance::GetAllocator());
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

BufferDescriptor GraphicsPipelineManager::CreateSceneDescriptor(uint32_t size) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();
    auto unlitGPO = UnlitGraphicsPipeline::GetResource();

    BufferDescriptor uniform;

    std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitGPO.sceneDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = layouts.data();

    uniform.descriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, uniform.descriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate scene descriptor sets!");

    uniform.buffers.resize(numFrames);

    for (size_t i = 0; i < numFrames; i++) {
        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = size;

        BufferManager::Create(uniformDesc, uniform.buffers[i]);
    }

    return uniform;
}

BufferDescriptor GraphicsPipelineManager::CreateMeshDescriptor(uint32_t size) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();
    auto unlitGPO = UnlitGraphicsPipeline::GetResource();

    BufferDescriptor uniform;

    std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitGPO.meshDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = GraphicsPipelineManager::meshDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = layouts.data();

    uniform.descriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, uniform.descriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate transform descriptor sets!");

    uniform.buffers.resize(numFrames);
    for (size_t i = 0; i < numFrames; i++) {

        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = size;

        BufferManager::Create(uniformDesc, uniform.buffers[i]);
    }
    
    return uniform;
}

BufferDescriptor GraphicsPipelineManager::CreateMaterialDescriptor(uint32_t size) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();
    auto unlitGPO = UnlitGraphicsPipeline::GetResource();

    BufferDescriptor uniform;

    std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitGPO.materialDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = GraphicsPipelineManager::materialDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = layouts.data();

    uniform.descriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, uniform.descriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate transform descriptor sets!");

    uniform.buffers.resize(numFrames);
    for (size_t i = 0; i < numFrames; i++) {

        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = size;

        BufferManager::Create(uniformDesc, uniform.buffers[i]);
    }
    
    return uniform;
}

TextureDescriptor GraphicsPipelineManager::CreateTextureDescriptor(VkDescriptorSetLayout setLayout) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();

    TextureDescriptor uniform;

    std::vector<VkDescriptorSetLayout> matLayouts(numFrames, setLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = textureDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = matLayouts.data();

    uniform.descriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, uniform.descriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate texture descriptor sets!");

    return uniform;
}

void GraphicsPipelineManager::UpdateBufferDescriptor(BufferDescriptor& descriptor, void* data, uint32_t size) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();

    for (size_t i = 0; i < numFrames; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = descriptor.buffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = size;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptor.descriptors[i];
        descriptorWrites[0].dstBinding = 0;
        // in the case of our descriptors being arrays, we specify the index
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void GraphicsPipelineManager::UpdateTextureDescriptor(TextureDescriptor& descriptor, TextureResource* texture) {
    std::array<VkWriteDescriptorSet, 1> writes{};

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture->image.view;
    imageInfo.sampler = texture->sampler;

    for (size_t i = 0; i < SwapChain::GetNumFrames(); i++) {
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptor.descriptors[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), (uint32_t)(writes.size()), writes.data(), 0, nullptr);
    }
}
