#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
#include "Texture.hpp"
#include "RenderingPass.hpp"
#include "AssetManager.hpp"
#include "VulkanLayer.h"

#include "imgui/imgui_impl_vulkan.h"

#include "FileManager.hpp"

namespace DeferredShading {

struct Context {
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

Context ctx;

void Setup() {
    {
        GraphicsPipelineManager::CreateDefaultDesc(lightPass.gpoDesc);
        lightPass.gpoDesc.name = "Light";
        lightPass.gpoDesc.shaderStages.resize(2);
        lightPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        lightPass.gpoDesc.shaderStages[0].path = "light.vert";
        lightPass.gpoDesc.shaderStages[0].entry_point = "main";
        lightPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        lightPass.gpoDesc.shaderStages[1].path = "light.frag";
        lightPass.gpoDesc.shaderStages[1].entry_point = "main";
        lightPass.gpoDesc.attributesDesc.clear();
        lightPass.gpoDesc.bindingDesc = {};
        lightPass.gpoDesc.useDepthAttachment = false;
        lightPass.gpoDesc.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM };
        lightPass.clearColors = { {0, 0, 0, 1} };
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(opaquePass.gpoDesc);
        opaquePass.gpoDesc.name = "Opaque";
        opaquePass.gpoDesc.shaderStages.resize(2);
        opaquePass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        opaquePass.gpoDesc.shaderStages[0].path = "opaque.vert";
        opaquePass.gpoDesc.shaderStages[0].entry_point = "main";
        opaquePass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        opaquePass.gpoDesc.shaderStages[1].path = "opaque.frag";
        opaquePass.gpoDesc.shaderStages[1].entry_point = "main";
        opaquePass.gpoDesc.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
        opaquePass.clearColors = { {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1} };
        opaquePass.gpoDesc.useDepthAttachment = true;
        opaquePass.gpoDesc.depthFormat = VK_FORMAT_D32_SFLOAT;
    }
    {
        GraphicsPipelineManager::CreateDefaultDesc(presentPass.gpoDesc);
        presentPass.gpoDesc.name = "Present";
        presentPass.gpoDesc.shaderStages.resize(2);
        presentPass.gpoDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        presentPass.gpoDesc.shaderStages[0].path = "present.vert";
        presentPass.gpoDesc.shaderStages[0].entry_point = "main";
        presentPass.gpoDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;
        presentPass.gpoDesc.shaderStages[1].path = "present.frag";
        presentPass.gpoDesc.shaderStages[1].entry_point = "main";
        presentPass.gpoDesc.colorFormats = { VK_FORMAT_B8G8R8A8_UNORM };
        presentPass.createAttachments = false;
        presentPass.gpoDesc.attributesDesc.clear();
        presentPass.gpoDesc.bindingDesc = {};
        presentPass.gpoDesc.useDepthAttachment = false;
    }
}

void Create() {
    VkDevice device = vkw::ctx().device;
    RenderingPassManager::Create();
    RenderingPassManager::CreateRenderingPass(lightPass);
    RenderingPassManager::CreateRenderingPass(opaquePass);
    RenderingPassManager::CreateRenderingPass(presentPass);
}

void Destroy() {
    RenderingPassManager::DestroyRenderingPass(opaquePass);
    RenderingPassManager::DestroyRenderingPass(presentPass);
    RenderingPassManager::DestroyRenderingPass(lightPass);
    RenderingPassManager::Destroy();
}

void ReloadShaders() {
    GraphicsPipelineManager::ReloadShaders(opaquePass.gpoDesc, opaquePass.gpo);
    GraphicsPipelineManager::ReloadShaders(presentPass.gpoDesc, presentPass.gpo);
    GraphicsPipelineManager::ReloadShaders(lightPass.gpoDesc, lightPass.gpo);
}

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId) {
    MeshResource& mesh = AssetManager::meshes[meshId];
    VkBuffer vertexBuffers[] = { mesh.vertexBuffer.GetBuffer()};
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
}

void BindConstants(VkCommandBuffer commandBuffer, RenderingPass& pass, void* data, u32 size) {
    vkCmdPushConstants(commandBuffer, pass.gpo.layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void BeginOpaquePass(VkCommandBuffer commandBuffer) {
    for (int i = 0; i < opaquePass.colorAttachments.size(); i++) {
        //VkImage image = opaquePass.colorAttachments[i].GetImage();
        //ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, image);
        vkw::CmdBarrier(opaquePass.colorAttachments[i], vkw::Layout::ColorAttachment);
    }
    //ImageManager::BarrierDepthUndefinedToAttachment(commandBuffer, opaquePass.depthAttachment.GetImage());
    vkw::CmdBarrier(opaquePass.depthAttachment, vkw::Layout::DepthAttachment);

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = vkw::ctx().swapChainExtent;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = opaquePass.colorAttachInfos.size();
    renderingInfo.pColorAttachments = opaquePass.colorAttachInfos.data();
    renderingInfo.pDepthAttachment = &opaquePass.depthAttachInfo;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePass.gpo.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
}

void EndPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRendering(commandBuffer);
}

void LightPass(VkCommandBuffer commandBuffer, LightConstants constants) {
    for (int i = 0; i < opaquePass.colorAttachments.size(); i++) {
        //VkImage res = opaquePass.colorAttachments[i].GetImage();
        //ImageManager::BarrierColorAttachmentToRead(commandBuffer, res);
        vkw::CmdBarrier(opaquePass.colorAttachments[i], vkw::Layout::ShaderRead);
    }
    //VkImage depthRes = opaquePass.depthAttachment.GetImage();
    //ImageManager::BarrierDepthAttachmentToRead(commandBuffer, depthRes);
    vkw::CmdBarrier(opaquePass.depthAttachment, vkw::Layout::DepthRead);

    constants.albedoRID = opaquePass.colorAttachments[0].rid;
    constants.normalRID = opaquePass.colorAttachments[1].rid;
    constants.materialRID = opaquePass.colorAttachments[2].rid;
    constants.emissionRID = opaquePass.colorAttachments[3].rid;
    constants.depthRID = opaquePass.depthAttachment.rid;

    //VkImage lightColorRes = lightPass.colorAttachments[0].GetImage();
    //ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, lightColorRes);
    vkw::CmdBarrier(lightPass.colorAttachments[0], vkw::Layout::ColorAttachment);

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = vkw::ctx().swapChainExtent;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = lightPass.colorAttachInfos.size();
    renderingInfo.pColorAttachments = lightPass.colorAttachInfos.data();
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, lightPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer) {
    //VkImage lightRes = lightPass.colorAttachments[0].GetImage();
    //ImageManager::BarrierColorAttachmentToRead(commandBuffer, lightRes);
    vkw::CmdBarrier(lightPass.colorAttachments[0], vkw::Layout::ShaderRead);

    PresentConstant constants;
    constants.lightRID = lightPass.colorAttachments[0].rid;
    constants.albedoRID = opaquePass.colorAttachments[0].rid;
    constants.normalRID = opaquePass.colorAttachments[1].rid;
    constants.materialRID = opaquePass.colorAttachments[2].rid;
    constants.emissionRID = opaquePass.colorAttachments[3].rid;
    constants.depthRID = opaquePass.depthAttachment.rid;

    constants.imageType = ctx.presentType;

    //vkw::CmdBarrier(vkw::ctx()., vkw::Layout::ShaderRead);
    ImageManager::BarrierColorUndefinedToAttachment(commandBuffer, vkw::ctx().GetCurrentSwapChainImage());

    VkRenderingAttachmentInfoKHR colorAttach{};
    colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttach.imageView = vkw::ctx().GetCurrentSwapChainView();
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.resolveMode = VK_RESOLVE_MODE_NONE;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = { 0, 0, 0, 1.0f };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea.extent = vkw::ctx().swapChainExtent;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.flags = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPass.gpo.pipeline);
    vkCmdPushConstants(commandBuffer, presentPass.gpo.layout, VK_SHADER_STAGE_ALL, 0, sizeof(constants), &constants);
    auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPass.gpo.layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

void EndPresentPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRendering(commandBuffer);
    ImageManager::BarrierColorAttachmentToPresent(commandBuffer, vkw::ctx().GetCurrentSwapChainImage());
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