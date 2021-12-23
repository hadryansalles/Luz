#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
#include "SwapChain.hpp"
#include "LogicalDevice.hpp"
#include "Texture.hpp"
#include "RenderingPass.hpp"

#include "imgui/imgui_impl_vulkan.h"

#include "FileManager.hpp"

namespace DeferredShading {

struct Context {
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRendering;
    PFN_vkCmdEndRenderingKHR vkCmdEndRendering;
    RenderingPass presentPass;
    const char* presentTypes[2] = { "Final Image", "Depth" };
    int presentType = 0;
};

struct PresentConstant {
    int imageRID;
    int imageType;
};

Context ctx;

void Setup() {
    GraphicsPipelineManager::CreateDefaultDesc(ctx.presentPass.gpoDesc);
    ctx.presentPass.gpoDesc.name = "Present";
    ctx.presentPass.gpoDesc.shaderStages.resize(2);
    ctx.presentPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
    ctx.presentPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
    std::vector<std::vector<char>> shaderBytes = FileManager::ReadShaders({"bin/present.vert.spv", "bin/present.frag.spv"});
    ctx.presentPass.gpoDesc.shaderStages[0].shaderBytes = shaderBytes[0];
    ctx.presentPass.gpoDesc.shaderStages[1].shaderBytes = shaderBytes[1];
    ctx.presentPass.gpoDesc.attributesDesc.clear();
    ctx.presentPass.gpoDesc.bindingDesc = {};
    ctx.presentPass.numColorAttachments = 1;
    ctx.presentPass.useDepthAttachment = true;
}

void Create() {
    VkDevice device = LogicalDevice::GetVkDevice();
    RenderingPassManager::Create();
    ctx.vkCmdBeginRendering = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
    ctx.vkCmdEndRendering = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
    RenderingPassManager::CreateRenderingPass(ctx.presentPass);
}

void Destroy() {
    VkDevice device = LogicalDevice::GetVkDevice();
    RenderingPassManager::DestroyRenderingPass(ctx.presentPass);
    RenderingPassManager::Destroy();
}

void BeginRendering(VkCommandBuffer commandBuffer, int numFrame) {
    ImageResource colorImage = RenderingPassManager::imageAttachments[ctx.presentPass.colorAttachments[0]];
    ImageResource depthImage = RenderingPassManager::imageAttachments[ctx.presentPass.depthAttachment];

    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, colorImage.image);
    ImageManager::BarrierDepthUndefinedToAttachment(commandBuffer, depthImage.image);

    VkRenderingAttachmentInfoKHR colorAttach{};
    colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttach.imageView = colorImage.view;
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = { 0, 0, 0, 1.0f };

    VkRenderingAttachmentInfoKHR depthAttach{};
    depthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depthAttach.imageView = depthImage.view;
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttach.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = SwapChain::GetExtent();
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;
    renderingInfo.pDepthAttachment = &depthAttach;
    renderingInfo.pStencilAttachment = &depthAttach;

    ctx.vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void EndRendering(VkCommandBuffer commandBuffer, int numFrame) {
    ctx.vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame) {
    ImageResource colorImage = RenderingPassManager::imageAttachments[ctx.presentPass.colorAttachments[0]];
    ImageResource depthImage = RenderingPassManager::imageAttachments[ctx.presentPass.depthAttachment];

    PresentConstant constants;
    if (ctx.presentType == 0) {
        ImageManager::BarrierColorAttachmentToRead(commandBuffer, colorImage.image);
        constants.imageRID = ctx.presentPass.colorAttachments[0];
    } else if (ctx.presentType == 1) {
        ImageManager::BarrierDepthAttachmentToRead(commandBuffer, depthImage.image);
        constants.imageRID = ctx.presentPass.depthAttachment;
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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.presentPass.gpoRes.pipeline);
    vkCmdPushConstants(commandBuffer, ctx.presentPass.gpoRes.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.presentPass.gpoRes.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

void EndPresentPass(VkCommandBuffer commandBuffer, int numFrame) {
    ctx.vkCmdEndRendering(commandBuffer);
    ImageManager::BarrierColorAttachmentToPresent(commandBuffer, SwapChain::GetImage(numFrame));
}

void OnImgui(int numFrame) {
    if (ImGui::Begin("Deferred Renderer")) {
        if (ImGui::BeginCombo("Present", ctx.presentTypes[ctx.presentType])) {
            bool selected = ctx.presentType == 0;
            if (ImGui::Selectable(ctx.presentTypes[0], &selected)) {
                ctx.presentType = 0;
            }
            selected = ctx.presentType == 1;
            if (ImGui::Selectable(ctx.presentTypes[1], &selected)) {
                ctx.presentType = 1;
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

}