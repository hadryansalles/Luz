#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
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

    light = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "light.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "light.frag"},
        },
        .extent = {vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height},
        .name = "Light",
        .vertexAttributes = {},
        .colorFormats = {vkw::Format::RGBA8_unorm},
        .useDepth = false,
    });
    opaque = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "opaque.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "opaque.frag"},
        },
        .extent = {vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height},
        .name = "Opaque",
        .vertexAttributes = {vkw::Format::RGB32_sfloat, vkw::Format::RGB32_sfloat, vkw::Format::RGBA32_sfloat, vkw::Format::RG32_sfloat},
        .colorFormats = {vkw::Format::RGBA8_unorm, vkw::Format::RGBA32_sfloat, vkw::Format::RGBA8_unorm, vkw::Format::RGBA8_unorm},
        .useDepth = true,
        .depthFormat = vkw::Format::D32_sfloat
    });
    present = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "present.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "present.frag"},
        },
        .extent = {vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height},
        .name = "Present",
        .vertexAttributes = {},
        .colorFormats = {vkw::Format::BGRA8_unorm},
        .useDepth = false,
    });
}

void Recreate() {
    RenderingPassManager::DestroyRenderingPass(lightPass);
    RenderingPassManager::DestroyRenderingPass(opaquePass);
    RenderingPassManager::DestroyRenderingPass(presentPass);

    RenderingPassManager::CreateRenderingPass(lightPass);
    RenderingPassManager::CreateRenderingPass(opaquePass);
    RenderingPassManager::CreateRenderingPass(presentPass);
}

void Destroy() {
    RenderingPassManager::DestroyRenderingPass(opaquePass);
    RenderingPassManager::DestroyRenderingPass(presentPass);
    RenderingPassManager::DestroyRenderingPass(lightPass);
    RenderingPassManager::Destroy();

    GraphicsPipelineManager::DestroyShaders(opaquePass.gpo);
    GraphicsPipelineManager::DestroyShaders(presentPass.gpo);
    GraphicsPipelineManager::DestroyShaders(lightPass.gpo);
    opaque = {};
    light = {};
    present = {};
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
        vkw::CmdBarrier(opaquePass.colorAttachments[i], vkw::Layout::ColorAttachment);
    }
    vkw::CmdBarrier(opaquePass.depthAttachment, vkw::Layout::DepthAttachment);
    vkw::CmdBeginRendering(opaquePass.colorAttachments, opaquePass.depthAttachment, { vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height });
    vkw::CmdBindPipeline(opaque);
}

void EndPass(VkCommandBuffer commandBuffer) {
    vkw::CmdEndRendering();
}

void LightPass(VkCommandBuffer commandBuffer, LightConstants constants) {
    for (int i = 0; i < opaquePass.colorAttachments.size(); i++) {
        vkw::CmdBarrier(opaquePass.colorAttachments[i], vkw::Layout::ShaderRead);
    }
    vkw::CmdBarrier(opaquePass.depthAttachment, vkw::Layout::DepthRead);
    vkw::CmdBarrier(lightPass.colorAttachments[0], vkw::Layout::ColorAttachment);

    constants.albedoRID = opaquePass.colorAttachments[0].rid;
    constants.normalRID = opaquePass.colorAttachments[1].rid;
    constants.materialRID = opaquePass.colorAttachments[2].rid;
    constants.emissionRID = opaquePass.colorAttachments[3].rid;
    constants.depthRID = opaquePass.depthAttachment.rid;

    vkw::CmdBeginRendering(lightPass.colorAttachments, {}, {vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height});
    vkw::CmdBindPipeline(light);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer) {
    vkw::CmdBarrier(lightPass.colorAttachments[0], vkw::Layout::ShaderRead);

    PresentConstant constants;
    constants.lightRID = lightPass.colorAttachments[0].rid;
    constants.albedoRID = opaquePass.colorAttachments[0].rid;
    constants.normalRID = opaquePass.colorAttachments[1].rid;
    constants.materialRID = opaquePass.colorAttachments[2].rid;
    constants.emissionRID = opaquePass.colorAttachments[3].rid;
    constants.depthRID = opaquePass.depthAttachment.rid;
    constants.imageType = ctx.presentType;

    vkw::CmdBarrier(vkw::ctx().GetCurrentSwapChainImage(), vkw::Layout::ColorAttachment);

    vkw::CmdBeginRendering({ vkw::ctx().GetCurrentSwapChainImage() }, {}, { vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height });
    vkw::CmdBindPipeline(present);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

void EndPresentPass(VkCommandBuffer commandBuffer) {
    vkw::CmdEndRendering();
    vkw::CmdBarrier(vkw::ctx().GetCurrentSwapChainImage(), vkw::Layout::Present);
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