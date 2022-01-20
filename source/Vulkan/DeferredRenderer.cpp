#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
#include "SwapChain.hpp"
#include "LogicalDevice.hpp"
#include "Texture.hpp"
#include "RenderingPass.hpp"
#include "AssetManager.hpp"

#include "imgui/imgui_impl_vulkan.h"

#include "FileManager.hpp"

namespace DeferredShading {

struct Context {
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRendering;
    PFN_vkCmdEndRenderingKHR vkCmdEndRendering;
    const char* presentTypes[3] = { "Albedo", "Normal", "Depth" };
    int presentType = 0;
};

struct PresentConstant {
    int imageRID;
    int imageType;
};

Context ctx;

void Setup() {
    {
        GraphicsPipelineManager::CreateDefaultDesc(opaquePass.gpoDesc);
        opaquePass.gpoDesc.name = "Opaque";
        opaquePass.gpoDesc.shaderStages.resize(2);
        opaquePass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        opaquePass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        std::vector<std::vector<char>> shaderBytes = FileManager::ReadShaders({"bin/opaque.vert.spv", "bin/opaque.frag.spv"});
        opaquePass.gpoDesc.shaderStages[0].shaderBytes = shaderBytes[0];
        opaquePass.gpoDesc.shaderStages[1].shaderBytes = shaderBytes[1];
        opaquePass.numColorAttachments = 2;
        opaquePass.useDepthAttachment = true;
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(presentPass.gpoDesc);
        presentPass.gpoDesc.name = "Present";
        presentPass.gpoDesc.shaderStages.resize(2);
        presentPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        presentPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        std::vector<std::vector<char>> shaderBytes = FileManager::ReadShaders({"bin/present.vert.spv", "bin/present.frag.spv"});
        presentPass.gpoDesc.shaderStages[0].shaderBytes = shaderBytes[0];
        presentPass.gpoDesc.shaderStages[1].shaderBytes = shaderBytes[1];
        presentPass.gpoDesc.attributesDesc.clear();
        presentPass.gpoDesc.bindingDesc = {};
        presentPass.numColorAttachments = 0;
        presentPass.useDepthAttachment = false;
    }
}

void Create() {
    VkDevice device = LogicalDevice::GetVkDevice();
    RenderingPassManager::Create();
    ctx.vkCmdBeginRendering = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
    ctx.vkCmdEndRendering = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
    RenderingPassManager::CreateRenderingPass(opaquePass);
    RenderingPassManager::CreateRenderingPass(presentPass);
}

void Destroy() {
    RenderingPassManager::DestroyRenderingPass(opaquePass);
    RenderingPassManager::DestroyRenderingPass(presentPass);
    RenderingPassManager::Destroy();
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

void BeginOpaquePass(VkCommandBuffer commandBuffer, RenderingPass& pass) {
    ImageResource albedoImage = RenderingPassManager::imageAttachments[opaquePass.colorAttachments[0]];
    ImageResource normalImage = RenderingPassManager::imageAttachments[opaquePass.colorAttachments[1]];
    ImageResource depthImage = RenderingPassManager::imageAttachments[opaquePass.depthAttachment];

    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, albedoImage.image);
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, normalImage.image);
    ImageManager::BarrierDepthUndefinedToAttachment(commandBuffer, depthImage.image);

    VkRenderingAttachmentInfoKHR albedoAttach{};
    albedoAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    albedoAttach.imageView = albedoImage.view;
    albedoAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    albedoAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    albedoAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    albedoAttach.clearValue.color = { 0, 0, 0, 1.0f };

    VkRenderingAttachmentInfoKHR normalAttach{};
    normalAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    normalAttach.imageView = normalImage.view;
    normalAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    normalAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    normalAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttach.clearValue.color = { 0, 0, 0, 1.0f };

    VkRenderingAttachmentInfoKHR depthAttach{};
    depthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depthAttach.imageView = depthImage.view;
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttach.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingAttachmentInfoKHR colorAttachs[] = { albedoAttach, normalAttach };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = COUNT_OF(colorAttachs);
    renderingInfo.pColorAttachments = colorAttachs;
    renderingInfo.pDepthAttachment = &depthAttach;
    renderingInfo.pStencilAttachment = &depthAttach;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);

    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.gpo.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
}

void EndPass(VkCommandBuffer commandBuffer) {
    ctx.vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame) {
    ImageResource albedoImage = RenderingPassManager::imageAttachments[opaquePass.colorAttachments[0]];
    ImageResource normalImage = RenderingPassManager::imageAttachments[opaquePass.colorAttachments[1]];
    ImageResource depthImage = RenderingPassManager::imageAttachments[opaquePass.depthAttachment];

    PresentConstant constants;
    if (ctx.presentType == 0) {
        ImageManager::BarrierColorAttachmentToRead(commandBuffer, albedoImage.image);
        constants.imageRID = opaquePass.colorAttachments[0];
    } else if (ctx.presentType == 1) {
        ImageManager::BarrierColorAttachmentToRead(commandBuffer, normalImage.image);
        constants.imageRID = opaquePass.colorAttachments[1];
    } else if (ctx.presentType == 2) {
        ImageManager::BarrierDepthAttachmentToRead(commandBuffer, depthImage.image);
        constants.imageRID = opaquePass.depthAttachment;
    }
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