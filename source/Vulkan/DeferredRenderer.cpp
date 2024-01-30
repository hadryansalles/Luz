#include "Luzpch.hpp"

#include "DeferredRenderer.hpp"
#include "AssetManager.hpp"
#include "VulkanLayer.h"

#include "imgui/imgui_impl_vulkan.h"

#include "FileManager.hpp"

namespace DeferredShading {

struct Context {
    const char* presentTypes[7] = { "Light", "Albedo", "Normal", "Material", "Emission", "Depth", "All"};
    int presentType = 0;

    vkw::Pipeline opaquePipeline;
    vkw::Pipeline lightPipeline;
    vkw::Pipeline presentPipeline;

    vkw::Image albedo;
    vkw::Image normal;
    vkw::Image material;
    vkw::Image emission;
    vkw::Image depth;
    vkw::Image light;
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
}

void Recreate(uint32_t width, uint32_t height) {
    ctx.albedo = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Albedo Attachment"
    });
    ctx.normal = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA32_sfloat,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Normal Attachment"
    });
    ctx.material = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Material Attachment"
    });
    ctx.emission = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Emission Attachment"
    });
    ctx.light = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Light Attachment"
    });
    ctx.depth = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::D32_sfloat,
        .usage = vkw::ImageUsage::DepthAttachment | vkw::ImageUsage::Sampled,
        .name = "Depth Attachment"
    });
    ctx.lightPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "light.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "light.frag"},
        },
        .extent = {width, height},
        .name = "Light Pipeline",
        .vertexAttributes = {},
        .colorFormats = {ctx.light.format},
        .useDepth = false,
    });
    ctx.opaquePipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "opaque.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "opaque.frag"},
        },
        .extent = {width, height},
        .name = "Opaque Pipeline",
        .vertexAttributes = {vkw::Format::RGB32_sfloat, vkw::Format::RGB32_sfloat, vkw::Format::RGBA32_sfloat, vkw::Format::RG32_sfloat},
        .colorFormats = {ctx.albedo.format, ctx.normal.format, ctx.material.format, ctx.emission.format},
        .useDepth = true,
        .depthFormat = {ctx.depth.format}
    });
    ctx.presentPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "present.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "present.frag"},
        },
        .extent = {width, height},
        .name = "Present Pipeline",
        .vertexAttributes = {},
        .colorFormats = {(vkw::Format)vkw::ctx().colorFormat},
        .useDepth = false,
    });
}

void Destroy() {
    ctx = {};
}

void ReloadShaders() {
    // todo: reload
}

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId) {
    MeshResource& mesh = AssetManager::meshes[meshId];
    VkBuffer vertexBuffers[] = { mesh.vertexBuffer.GetBuffer()};
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
}

void BeginOpaquePass(VkCommandBuffer commandBuffer) {
    std::vector<vkw::Image> attachs = { ctx.albedo, ctx.normal, ctx.material, ctx.emission };
    for (auto& attach : attachs) {
        vkw::CmdBarrier(attach, vkw::Layout::ColorAttachment);
    }
    vkw::CmdBarrier(ctx.depth, vkw::Layout::DepthAttachment);
    vkw::CmdBeginRendering(attachs, ctx.depth, { vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height });
    vkw::CmdBindPipeline(ctx.opaquePipeline);
}

void EndPass(VkCommandBuffer commandBuffer) {
    vkw::CmdEndRendering();
}

void LightPass(VkCommandBuffer commandBuffer, LightConstants constants) {
    std::vector<vkw::Image> attachs = { ctx.albedo, ctx.normal, ctx.material, ctx.emission };
    for (auto& attach : attachs) {
        vkw::CmdBarrier(attach, vkw::Layout::ShaderRead);
    }
    vkw::CmdBarrier(ctx.depth, vkw::Layout::DepthRead);
    vkw::CmdBarrier(ctx.light, vkw::Layout::ColorAttachment);

    constants.albedoRID = ctx.albedo.rid;
    constants.normalRID = ctx.normal.rid;
    constants.materialRID = ctx.material.rid;
    constants.emissionRID = ctx.emission.rid;
    constants.depthRID = ctx.depth.rid;

    vkw::CmdBeginRendering({ctx.light}, {}, {vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height});
    vkw::CmdBindPipeline(ctx.lightPipeline);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
}

void BeginPresentPass(VkCommandBuffer commandBuffer) {
    vkw::CmdBarrier(ctx.light, vkw::Layout::ShaderRead);

    PresentConstant constants;
    constants.lightRID = ctx.light.rid;
    constants.albedoRID = ctx.albedo.rid;
    constants.normalRID = ctx.normal.rid;
    constants.materialRID = ctx.material.rid;
    constants.emissionRID = ctx.emission.rid;
    constants.depthRID = ctx.depth.rid;
    constants.imageType = ctx.presentType;

    vkw::CmdBarrier(vkw::ctx().GetCurrentSwapChainImage(), vkw::Layout::ColorAttachment);
    vkw::CmdBeginRendering({ vkw::ctx().GetCurrentSwapChainImage() }, {}, { vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height });
    vkw::CmdBindPipeline(ctx.presentPipeline);
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