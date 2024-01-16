#include "Luzpch.hpp"

#include "PBRGraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "VulkanLayer.h"
#include "AssetManager.hpp"

void PBRGraphicsPipeline::Setup() {
    desc.name = "PBR";

    desc.shaderStages.resize(2);
    desc.shaderStages[0].path = "opaque.vert";
    desc.shaderStages[0].entry_point = "main";
    desc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
    desc.shaderStages[1].path = "opaque.frag";
    desc.shaderStages[1].entry_point = "main";
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
    desc.multisampling.rasterizationSamples = vkw::ctx().numSamples;
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

    // desc.colorBlendAttachments.resize(1);

    // desc.colorBlendAttachments[0].colorWriteMask =  VK_COLOR_COMPONENT_R_BIT;
    // desc.colorBlendAttachments[0].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    // desc.colorBlendAttachments[0].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    // desc.colorBlendAttachments[0].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    // desc.colorBlendAttachments[0].blendEnable = VK_TRUE;
    // desc.colorBlendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    // desc.colorBlendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    // desc.colorBlendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    // desc.colorBlendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    // desc.colorBlendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    // desc.colorBlendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

    // desc.colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // desc.colorBlendState.logicOpEnable = VK_FALSE;
    // desc.colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    // desc.colorBlendState.attachmentCount = desc.colorBlendAttachments.size();
    // desc.colorBlendState.pAttachments = desc.colorBlendAttachments.data();
    // desc.colorBlendState.blendConstants[0] = 0.0f;
    // desc.colorBlendState.blendConstants[1] = 0.0f;
    // desc.colorBlendState.blendConstants[2] = 0.0f;
    // desc.colorBlendState.blendConstants[3] = 0.0f;
}

void PBRGraphicsPipeline::Create() {
    desc.multisampling.rasterizationSamples = vkw::ctx().numSamples;
    GraphicsPipelineManager::CreatePipeline(desc, res);
}

void PBRGraphicsPipeline::Destroy() {
    GraphicsPipelineManager::DestroyPipeline(res);
}

void PBRGraphicsPipeline::OnImgui() {
    GraphicsPipelineManager::OnImgui(desc, res);
}
