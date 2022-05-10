#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
#include "SwapChain.hpp"
#include "LogicalDevice.hpp"
#include "Texture.hpp"
#include "RenderingPass.hpp"
#include "AssetManager.hpp"
#include "Scene.hpp"

#include "imgui/imgui_impl_vulkan.h"

#include "FileManager.hpp"

namespace DeferredShading {

struct Context {
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRendering;
    PFN_vkCmdEndRenderingKHR vkCmdEndRendering;
    const char* presentTypes[7] = { "Light", "Albedo", "Normal", "Material", "Emission", "Depth", "All"};
    int presentType = 0;
};

struct PresentConstant {
    int imageType;
    int lightRID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
};

struct PanoramaToCubeConstants {
    int panoramaRID;
    int face;
};

Context ctx;

void Setup() {
    {
        GraphicsPipelineManager::CreateDefaultDesc(lightPass.gpoDesc);
        lightPass.gpoDesc.name = "Light";
        lightPass.gpoDesc.shaderStages.resize(2);
        lightPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        lightPass.gpoDesc.shaderStages[0].path = "bin/quadPass.vert.spv";
        lightPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        lightPass.gpoDesc.shaderStages[1].path = "bin/light.frag.spv";
        lightPass.gpoDesc.attributesDesc.clear();
        lightPass.gpoDesc.bindingDesc = {};
        lightPass.gpoDesc.useDepthAttachment = false;
        lightPass.gpoDesc.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        lightPass.clearColors = { {0, 0, 0, 1} };
        renderingPasses.push_back(&lightPass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(panoramaToCubePass.gpoDesc);
        panoramaToCubePass.gpoDesc.name = "PanoramaToCubePass";
        panoramaToCubePass.gpoDesc.shaderStages.resize(2);
        panoramaToCubePass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        panoramaToCubePass.gpoDesc.shaderStages[0].path = "bin/panoramaToCube.vert.spv";
        panoramaToCubePass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        panoramaToCubePass.gpoDesc.shaderStages[1].path = "bin/panoramaToCube.frag.spv";
        panoramaToCubePass.gpoDesc.useDepthAttachment = false;
        panoramaToCubePass.crateAttachments = false;
        panoramaToCubePass.gpoDesc.attributesDesc.clear();
        panoramaToCubePass.gpoDesc.bindingDesc = {};
        panoramaToCubePass.gpoDesc.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        panoramaToCubePass.gpoDesc.dynamicViewportScissor = true;
        renderingPasses.push_back(&panoramaToCubePass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(envmapPass.gpoDesc);
        envmapPass.gpoDesc.name = "Envmap";
        envmapPass.gpoDesc.shaderStages.resize(2);
        envmapPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        envmapPass.gpoDesc.shaderStages[0].path = "bin/envmap.vert.spv";
        envmapPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        envmapPass.gpoDesc.shaderStages[1].path = "bin/envmap.frag.spv";
        envmapPass.gpoDesc.useDepthAttachment = false;
        envmapPass.crateAttachments = false;
        envmapPass.gpoDesc.attributesDesc.clear();
        envmapPass.gpoDesc.bindingDesc = {};
        envmapPass.gpoDesc.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        renderingPasses.push_back(&envmapPass);
        // envmapPass.clearColors = { {0, 0, 0, 1} };
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(opaquePass.gpoDesc);
        opaquePass.gpoDesc.name = "Opaque";
        opaquePass.gpoDesc.shaderStages.resize(2);
        opaquePass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        opaquePass.gpoDesc.shaderStages[0].path = "bin/opaque.vert.spv";
        opaquePass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        opaquePass.gpoDesc.shaderStages[1].path = "bin/opaque.frag.spv";
        opaquePass.gpoDesc.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
        opaquePass.clearColors = { {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1} };
        opaquePass.gpoDesc.useDepthAttachment = true;
        opaquePass.gpoDesc.depthFormat = VK_FORMAT_D32_SFLOAT;
        renderingPasses.push_back(&opaquePass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(presentPass.gpoDesc);
        presentPass.gpoDesc.name = "Present";
        presentPass.gpoDesc.shaderStages.resize(2);
        presentPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        presentPass.gpoDesc.shaderStages[0].path = "bin/quadPass.vert.spv";
        presentPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        presentPass.gpoDesc.shaderStages[1].path = "bin/present.frag.spv";
        // std::vector<std::vector<char>> shaderBytes = FileManager::ReadShaders({"bin/present.vert.spv", "bin/present.frag.spv"});
        // presentPass.gpoDesc.shaderStages[0].shaderBytes = shaderBytes[0];
        // presentPass.gpoDesc.shaderStages[1].shaderBytes = shaderBytes[1];
        presentPass.gpoDesc.colorFormats = { VK_FORMAT_B8G8R8A8_UNORM };
        presentPass.crateAttachments = false;
        presentPass.gpoDesc.attributesDesc.clear();
        presentPass.gpoDesc.bindingDesc = {};
        presentPass.gpoDesc.useDepthAttachment = false;
        renderingPasses.push_back(&presentPass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(shadowPass.gpoDesc);
        shadowPass.gpoDesc.name = "Shadow";
        shadowPass.gpoDesc.shaderStages.resize(2);
        shadowPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        shadowPass.gpoDesc.shaderStages[0].path = "bin/quadPass.vert.spv";
        shadowPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        shadowPass.gpoDesc.shaderStages[1].path = "bin/shadow.frag.spv";
        shadowPass.gpoDesc.colorFormats = { VK_FORMAT_R32_SFLOAT };
        shadowPass.crateAttachments = false;
        shadowPass.gpoDesc.attributesDesc.clear();
        shadowPass.gpoDesc.bindingDesc = {};
        shadowPass.gpoDesc.useDepthAttachment = false;
        renderingPasses.push_back(&shadowPass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(shadowBlurPass.gpoDesc);
        shadowBlurPass.gpoDesc.name = "ShadowBlur";
        shadowBlurPass.gpoDesc.shaderStages.resize(2);
        shadowBlurPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        shadowBlurPass.gpoDesc.shaderStages[0].path = "bin/quadPass.vert.spv";
        shadowBlurPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        shadowBlurPass.gpoDesc.shaderStages[1].path = "bin/shadowBlur.frag.spv";
        shadowBlurPass.gpoDesc.colorFormats = { VK_FORMAT_R32_SFLOAT };
        shadowBlurPass.clearColors = { {0} };
        shadowBlurPass.crateAttachments = true;
        shadowBlurPass.gpoDesc.attributesDesc.clear();
        shadowBlurPass.gpoDesc.bindingDesc = {};
        shadowBlurPass.gpoDesc.useDepthAttachment = false;
        renderingPasses.push_back(&shadowBlurPass);
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(volumetricLightPass.gpoDesc);
        volumetricLightPass.gpoDesc.name = "Volumetric Light";
        volumetricLightPass.gpoDesc.shaderStages.resize(2);
        volumetricLightPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        volumetricLightPass.gpoDesc.shaderStages[0].path = "bin/quadPass.vert.spv";
        volumetricLightPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        volumetricLightPass.gpoDesc.shaderStages[1].path = "bin/volumetricLight.frag.spv";
        volumetricLightPass.gpoDesc.colorFormats = { VK_FORMAT_R32G32B32A32_SFLOAT };
        volumetricLightPass.clearColors = { {0, 0, 0, 1} };
        volumetricLightPass.crateAttachments = true;
        volumetricLightPass.gpoDesc.attributesDesc.clear();
        volumetricLightPass.gpoDesc.bindingDesc = {};
        volumetricLightPass.gpoDesc.useDepthAttachment = false;
        renderingPasses.push_back(&volumetricLightPass);
    }
}

void Create() {
    VkDevice device = LogicalDevice::GetVkDevice();
    RenderingPassManager::Create();
    ctx.vkCmdBeginRendering = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
    ctx.vkCmdEndRendering = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
    for (RenderingPass* pass : renderingPasses) {
        RenderingPassManager::CreateRenderingPass(*pass);
    }
}

void Destroy() {
    for (RenderingPass* pass : renderingPasses) {
        RenderingPassManager::DestroyRenderingPass(*pass);
    }
    RenderingPassManager::Destroy();
}

void ReloadShaders() {
    for (RenderingPass* pass : renderingPasses) {
        GraphicsPipelineManager::ReloadShaders(pass->gpoDesc, pass->gpo);
    }
}

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId) {
    MeshResource& mesh = AssetManager::meshes[meshId];
    VkBuffer vertexBuffers[] = { mesh.vertexBuffer.buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
}

void BindConstants(VkCommandBuffer commandBuffer, RenderingPass& pass, void* data, u32 size) {
    vkCmdPushConstants(commandBuffer, pass.gpo.layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void BeginOpaquePass(VkCommandBuffer commandBuffer) {
    for (int i = 0; i < opaquePass.colorAttachments.size(); i++) {
        ImageResource image = RenderingPassManager::imageAttachments[opaquePass.colorAttachments[i]];
        ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, image.image);
    }
    ImageManager::BarrierDepthUndefinedToAttachment(commandBuffer, RenderingPassManager::imageAttachments[opaquePass.depthAttachment].image);

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = opaquePass.colorAttachInfos.size();
    renderingInfo.pColorAttachments = opaquePass.colorAttachInfos.data();
    renderingInfo.pDepthAttachment = &opaquePass.depthAttachInfo;
    renderingInfo.pStencilAttachment = &opaquePass.depthAttachInfo;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);

    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePass.gpo.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
}

void EndPass(VkCommandBuffer commandBuffer) {
    ctx.vkCmdEndRendering(commandBuffer);
}

void VolumetricLightPass(VkCommandBuffer commandBuffer, LightConstants constants) {
    ImageResource volRes = RenderingPassManager::imageAttachments[volumetricLightPass.colorAttachments[0]];
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, volRes.image);

    constants.depthRID = opaquePass.depthAttachment;

    VkRenderingAttachmentInfoKHR volAttach = {};
    volAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    volAttach.imageView = volRes.view;
    volAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    volAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    volAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    volAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    volAttach.clearValue.color = { 0 };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &volAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumetricLightPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, volumetricLightPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumetricLightPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    ctx.vkCmdEndRendering(commandBuffer);

    ImageManager::BarrierColorAttachmentToRead(commandBuffer, volRes.image);
}

void ShadowPass(VkCommandBuffer commandBuffer, ShadowConstants constants, Light* light) {
    ImageResource shadowRes = RenderingPassManager::imageAttachments[light->block.shadowBufferRID];
    ImageResource blurRes = RenderingPassManager::imageAttachments[shadowBlurPass.colorAttachments[0]];
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, shadowRes.image);
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, blurRes.image);

    constants.normalRID = opaquePass.colorAttachments[1];
    constants.depthRID = opaquePass.depthAttachment;

    {
        VkRenderingAttachmentInfoKHR shadowAttach = {};
        shadowAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        shadowAttach.imageView = blurRes.view;
        shadowAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        shadowAttach.resolveMode = VK_RESOLVE_MODE_NONE;
        shadowAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        shadowAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        shadowAttach.clearValue.color = { 0 };

        VkRenderingInfoKHR renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderingInfo.viewMask = 0;
        renderingInfo.layerCount = 1;
        renderingInfo.renderArea.extent = SwapChain::GetExtent();
        renderingInfo.renderArea.offset = { 0, 0 };
        renderingInfo.flags = 0;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &shadowAttach;
        renderingInfo.pDepthAttachment = nullptr;
        renderingInfo.pStencilAttachment = nullptr;

        ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPass.gpo.pipeline);
        vkCmdPushConstants(commandBuffer, shadowPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
        auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        ctx.vkCmdEndRendering(commandBuffer);
    }

    ImageManager::BarrierColorAttachmentToRead(commandBuffer, blurRes.image);

    {
        glm::vec2 deltaXY(1.0f / SwapChain::GetExtent().width, 1.0f / SwapChain::GetExtent().height);
        BlurConstants blurConst;
        blurConst.imageRID = shadowBlurPass.colorAttachments[0];
        blurConst.normalRID = opaquePass.colorAttachments[1];
        blurConst.depthRID = opaquePass.depthAttachment;
        blurConst.steps = light->shadowBlurSize;
        blurConst.delta = glm::vec2(deltaXY.x, 0.0f);
        VkRenderingAttachmentInfoKHR blurAttach = {};
        blurAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        blurAttach.imageView = shadowRes.view;
        blurAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        blurAttach.resolveMode = VK_RESOLVE_MODE_NONE;
        blurAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        blurAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        blurAttach.clearValue.color = { 0 };

        VkRenderingInfoKHR renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderingInfo.viewMask = 0;
        renderingInfo.layerCount = 1;
        renderingInfo.renderArea.extent = SwapChain::GetExtent();
        renderingInfo.renderArea.offset = { 0, 0 };
        renderingInfo.flags = 0;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &blurAttach;
        renderingInfo.pDepthAttachment = nullptr;
        renderingInfo.pStencilAttachment = nullptr;

        ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowBlurPass.gpo.pipeline);
        vkCmdPushConstants(commandBuffer, shadowBlurPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(blurConst), &blurConst);
        auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowBlurPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        ctx.vkCmdEndRendering(commandBuffer);

        VkImageCopy region{};
        region.srcOffset = { 0, 0, 0 };
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcSubresource.mipLevel = 0;
        region.dstOffset = { 0, 0, 0 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstSubresource.mipLevel = 0;
        region.extent = { SwapChain::GetExtent().width, SwapChain::GetExtent().height, 1 };

        ImageManager::BarrierColorAttachmentToRead(commandBuffer, shadowRes.image);
        ImageManager::Copy(commandBuffer, shadowRes, blurRes, region);
        ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, shadowRes.image);

        blurConst.delta = glm::vec2(0.0f, deltaXY.y);
        ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowBlurPass.gpo.pipeline);
        vkCmdPushConstants(commandBuffer, shadowBlurPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(blurConst), &blurConst);
        // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowBlurPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        ctx.vkCmdEndRendering(commandBuffer);
    }

    ImageManager::BarrierColorAttachmentToRead(commandBuffer, shadowRes.image);
}

void EnvmapPass(VkCommandBuffer commandBuffer, OpaqueConstants constants) {
    ImageResource lightColorRes = RenderingPassManager::imageAttachments[lightPass.colorAttachments[0]];
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, lightColorRes.image);

    VkRenderingAttachmentInfoKHR lightAttach = {};
    lightAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    lightAttach.imageView = lightColorRes.view;
    lightAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    lightAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    lightAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    lightAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    lightAttach.clearValue.color = lightPass.clearColors[0];

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &lightAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, envmapPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, envmapPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, envmapPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    ctx.vkCmdEndRendering(commandBuffer);
}

void LightPass(VkCommandBuffer commandBuffer, LightConstants constants) {
    // ImageResource envmapRes = RenderingPassManager::imageAttachments[envmapPass.colorAttachments[0]];
    // ImageManager::BarrierColorAttachmentToRead(commandBuffer, envmapRes.image);

    constants.albedoRID = opaquePass.colorAttachments[0];
    constants.normalRID = opaquePass.colorAttachments[1];
    constants.materialRID = opaquePass.colorAttachments[2];
    constants.emissionRID = opaquePass.colorAttachments[3];
    constants.depthRID = opaquePass.depthAttachment;
    constants.volumetricLightRID = volumetricLightPass.colorAttachments[0];

    ImageResource lightColorRes = RenderingPassManager::imageAttachments[lightPass.colorAttachments[0]];
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, lightColorRes.image);

    VkRenderingAttachmentInfoKHR lightAttach = {};
    lightAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    lightAttach.imageView = lightColorRes.view;
    lightAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    lightAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    lightAttach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    lightAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &lightAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, lightPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    ctx.vkCmdEndRendering(commandBuffer);
}

void RenderPanoramaToFace(VkCommandBuffer commandBuffer, RID panoramaRID, ImageResource face, int i) {

    PanoramaToCubeConstants constants;
    constants.face = i;
    constants.panoramaRID = panoramaRID;

    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, face.image);

    VkRenderingAttachmentInfoKHR faceAttach = {};
    faceAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    faceAttach.imageView = face.view;
    faceAttach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    faceAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    faceAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    faceAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    faceAttach.clearValue.color = { 0, 0, 0, 0 };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = { face.width, face.height };
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &faceAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, panoramaToCubePass.gpo.pipeline);
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = face.width;
    viewport.height = face.height;
    viewport.minDepth = 0.0f;
    viewport.minDepth = 1.0f;
    VkRect2D scissor{};
    scissor = renderingInfo.renderArea;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdPushConstants(commandBuffer, panoramaToCubePass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, panoramaToCubePass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    ctx.vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame) {
    ImageResource& lightRes = RenderingPassManager::imageAttachments[lightPass.colorAttachments[0]];
    ImageManager::BarrierColorAttachmentToRead(commandBuffer, lightRes.image);

    PresentConstant constants;
    constants.lightRID = lightPass.colorAttachments[0];
    constants.albedoRID = opaquePass.colorAttachments[0];
    constants.normalRID = opaquePass.colorAttachments[1];
    constants.materialRID = opaquePass.colorAttachments[2];
    constants.emissionRID = opaquePass.colorAttachments[3];
    constants.depthRID = opaquePass.depthAttachment;

    constants.imageType = ctx.presentType;

    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, SwapChain::GetImage(numFrame));

    VkRenderingAttachmentInfoKHR colorAttach{};
    colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttach.imageView = SwapChain::GetView(numFrame);
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = { 0, 0, 0, 1.0f };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, presentPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

void EndPresentPass(VkCommandBuffer commandBuffer, int numFrame) {
    ctx.vkCmdEndRendering(commandBuffer);
    ImageManager::BarrierColorAttachmentToPresent(commandBuffer, SwapChain::GetImage(numFrame));
}

void OnImgui(int numFrame) {
    if (ImGui::Begin("Deferred Renderer")) {
        if (ImGui::BeginCombo("Present", ctx.presentTypes[ctx.presentType])) {
            for (int i = 0; i < COUNT_OF(ctx.presentTypes); i++) {
                bool selected = ctx.presentType == i;
                if (ImGui::Selectable(ctx.presentTypes[i], &selected)) {
                    ctx.presentType = i;
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

}