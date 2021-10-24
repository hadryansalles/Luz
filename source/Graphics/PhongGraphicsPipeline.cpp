#include "Luzpch.hpp"

#include "PhongGraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "SwapChain.hpp"
#include "MeshManager.hpp"
#include "SceneManager.hpp"

void PhongGraphicsPipeline::Setup() {
    desc.name = "Phong";

    desc.shaderStages.resize(2);
    desc.shaderStages[0].shaderBytes = FileManager::ReadRawBytes("bin/phong.vert.spv");
    desc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
    desc.shaderStages[1].shaderBytes = FileManager::ReadRawBytes("bin/phong.frag.spv");
    desc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;

    desc.bindingDesc = MeshVertex::getBindingDescription();
    desc.attributesDesc = MeshVertex::getAttributeDescriptions();

    desc.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // fragments beyond near and far planes are clamped to them
    desc.rasterizer.depthClampEnable = VK_FALSE;
    desc.rasterizer.rasterizerDiscardEnable = VK_FALSE;
    desc.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // line thickness in terms of number of fragments
    desc.rasterizer.lineWidth = 1.0f;
    desc.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    desc.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.rasterizer.depthBiasEnable = VK_FALSE;
    desc.rasterizer.depthBiasConstantFactor = 0.0f;
    desc.rasterizer.depthBiasClamp = 0.0f;
    desc.rasterizer.depthBiasSlopeFactor = 0.0f;

    desc.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    desc.multisampling.rasterizationSamples = SwapChain::GetNumSamples();
    desc.multisampling.sampleShadingEnable = VK_FALSE;
    desc.multisampling.minSampleShading = 0.5f;
    desc.multisampling.pSampleMask = nullptr;

    desc.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    desc.depthStencil.depthTestEnable = VK_TRUE;
    desc.depthStencil.depthWriteEnable = VK_TRUE;
    desc.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    desc.depthStencil.depthBoundsTestEnable = VK_FALSE;
    desc.depthStencil.minDepthBounds = 0.0f;
    desc.depthStencil.maxDepthBounds = 1.0f;
    desc.depthStencil.stencilTestEnable = VK_FALSE;
    desc.depthStencil.front = {};
    desc.depthStencil.back = {};

    desc.colorBlendAttachment.colorWriteMask =  VK_COLOR_COMPONENT_R_BIT;
    desc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    desc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    desc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    desc.colorBlendAttachment.blendEnable = VK_TRUE;
    desc.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    desc.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    desc.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    desc.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    desc.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    desc.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    desc.colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    desc.colorBlendState.logicOpEnable = VK_FALSE;
    desc.colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    desc.colorBlendState.attachmentCount = 1;
    desc.colorBlendState.pAttachments = &desc.colorBlendAttachment;
    desc.colorBlendState.blendConstants[0] = 0.0f;
    desc.colorBlendState.blendConstants[1] = 0.0f;
    desc.colorBlendState.blendConstants[2] = 0.0f;
    desc.colorBlendState.blendConstants[3] = 0.0f;

    desc.bindings.resize(5);
    desc.bindings[0].binding = 0;
    desc.bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc.bindings[0].descriptorCount = 1;
    desc.bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    desc.bindings[1].binding = 0;
    desc.bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc.bindings[1].descriptorCount = 1;
    desc.bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    desc.bindings[2].binding = 0;
    desc.bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc.bindings[2].descriptorCount = 1;
    desc.bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    desc.bindings[3].binding = 0;
    desc.bindings[3].descriptorCount = 1;
    desc.bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // only relevant for image sampling related descriptors
    desc.bindings[3].pImmutableSamplers = nullptr;
    // here we specify in which shader stages the buffer will by referenced
    desc.bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    desc.bindings[4].binding = 0;
    desc.bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc.bindings[4].descriptorCount = 1;
    desc.bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
}

void PhongGraphicsPipeline::Create() {
    desc.multisampling.rasterizationSamples = SwapChain::GetNumSamples();
    GraphicsPipelineManager::CreatePipeline(desc, res);
}

void PhongGraphicsPipeline::Destroy() {
    GraphicsPipelineManager::DestroyPipeline(res);
}

void PhongGraphicsPipeline::OnImgui() {
    GraphicsPipelineManager::OnImgui(desc, res);
}

BufferDescriptor PhongGraphicsPipeline::CreateSceneDescriptor() {
    return GraphicsPipelineManager::CreateBufferDescriptor(res.descriptorSetLayouts[0], sizeof(SceneUBO));
}

BufferDescriptor PhongGraphicsPipeline::CreateModelDescriptor() {
    return GraphicsPipelineManager::CreateBufferDescriptor(res.descriptorSetLayouts[1], sizeof(ModelUBO));
}

BufferDescriptor PhongGraphicsPipeline::CreateMaterialDescriptor() {
    return GraphicsPipelineManager::CreateBufferDescriptor(res.descriptorSetLayouts[2], sizeof(PhongMaterialUBO));
}

TextureDescriptor PhongGraphicsPipeline::CreateTextureDescriptor() {
    return GraphicsPipelineManager::CreateTextureDescriptor(res.descriptorSetLayouts[3]);
}

BufferDescriptor PhongGraphicsPipeline::CreatePointLightDescriptor() {
    return GraphicsPipelineManager::CreateBufferDescriptor(res.descriptorSetLayouts[4], sizeof(PointLightUBO));
}
