#include "Luzpch.hpp"

#include "GraphicsPipelineManager.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "SwapChain.hpp"
#include "Instance.hpp"
#include "VulkanUtils.hpp"

void GraphicsPipelineManager::Create() {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();
    auto numFrames = SwapChain::GetNumFrames();
    uint32_t sets = static_cast<uint32_t>(numFrames);

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(numFrames);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(numFrames + 1);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 2*static_cast<uint32_t>(numFrames);
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    auto result = vkCreateDescriptorPool(device, &poolInfo, allocator, &descriptorPool);
    DEBUG_VK(result, "Failed to create descriptor pool!");
}

void GraphicsPipelineManager::Destroy() {
    vkDestroyDescriptorPool(LogicalDevice::GetVkDevice(), descriptorPool, Instance::GetAllocator());
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
    descriptorLayoutInfo.bindingCount = (uint32_t)(desc.bindings.size());
    descriptorLayoutInfo.pBindings = desc.bindings.data();

    auto vkRes = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &res.descriptorSetLayout);
    DEBUG_VK(vkRes, "Failed to create descriptor set layout!");

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &res.descriptorSetLayout;
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
    vkDestroyDescriptorSetLayout(LogicalDevice::GetVkDevice(), res.descriptorSetLayout, Instance::GetAllocator());
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
