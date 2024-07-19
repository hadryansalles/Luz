#include "Luzpch.hpp"

#include "GPUScene.hpp"
#include "AssetManager.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanWrapper.h"
#include "LuzCommon.h"

#include "FileManager.hpp"
#include <imgui/ImGuizmo.h>

namespace DeferredShading {

struct Context {
    vkw::Pipeline opaquePipeline;
    vkw::Pipeline lightPipeline;
    vkw::Pipeline composePipeline;
    vkw::Pipeline shadowMapPipeline;
    vkw::Pipeline ssvlPipeline;
    vkw::Pipeline shadowMapVolumetricLightPipeline;

    vkw::Image albedo;
    vkw::Image normal;
    vkw::Image material;
    vkw::Image emission;
    vkw::Image depth;
    vkw::Image light;
    vkw::Image compose;
};

struct ComposeConstant {
    int imageType;
    int lightRID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
};

Context ctx;

void CreateShaders() {
    if (ctx.albedo.format == 0) {
        LOG_CRITICAL("CREATE IMAGES BEFORE SHADERS IN DEFERRED RENDERER");
    }
    ctx.lightPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "light.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "light.frag"},
        },
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
        .name = "Opaque Pipeline",
        .vertexAttributes = {vkw::Format::RGB32_sfloat, vkw::Format::RGB32_sfloat, vkw::Format::RGBA32_sfloat, vkw::Format::RG32_sfloat},
        .colorFormats = {ctx.albedo.format, ctx.normal.format, ctx.material.format, ctx.emission.format},
        .useDepth = true,
        .depthFormat = {ctx.depth.format}
    });
    ctx.shadowMapPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "shadowMap.vert"},
            {.stage = vkw::ShaderStage::Geometry, .path = "shadowMap.geom"},
            {.stage = vkw::ShaderStage::Fragment, .path = "shadowMap.frag"},
        },
        .name = "ShadowMap Pipeline",
        .vertexAttributes = {vkw::Format::RGB32_sfloat, vkw::Format::RGB32_sfloat, vkw::Format::RGBA32_sfloat, vkw::Format::RG32_sfloat},
        .colorFormats = { },
        .useDepth = true,
        .depthFormat = { vkw::Format::D32_sfloat },
        .cullFront = true,
    });
    ctx.composePipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "present.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "present.frag"},
        },
        .name = "Present Pipeline",
        .vertexAttributes = {},
        .colorFormats = {vkw::Format::BGRA8_unorm},
        .useDepth = false,
    });
    ctx.ssvlPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Compute,
        .stages = {
            {.stage = vkw::ShaderStage::Compute, .path = "screenSpaceVolumetricLight.comp"},
        },
        .name = "VolumetricLight Pipeline",
    });
    ctx.shadowMapVolumetricLightPipeline = vkw::CreatePipeline({
        .point = vkw::PipelinePoint::Compute,
        .stages = {
            {.stage = vkw::ShaderStage::Compute, .path = "shadowMapVolumetricLight.comp"},
        },
        .name = "ShadowMapVolumetricLight Pipeline",
    });
}

void CreateImages(uint32_t width, uint32_t height) {
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
        .format = vkw::Format::RGBA32_sfloat,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled | vkw::ImageUsage::Storage,
        .name = "Light Attachment"
    });
    ctx.depth = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::D32_sfloat,
        .usage = vkw::ImageUsage::DepthAttachment | vkw::ImageUsage::Sampled,
        .name = "Depth Attachment"
    });
    ctx.compose = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::BGRA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Compose Attachment"
    });
}

void Destroy() {
    ctx = {};
}

void BeginOpaquePass() {
    std::vector<vkw::Image> attachs = { ctx.albedo, ctx.normal, ctx.material, ctx.emission };
    for (auto& attach : attachs) {
        vkw::CmdBarrier(attach, vkw::Layout::ColorAttachment);
    }
    vkw::CmdBarrier(ctx.depth, vkw::Layout::DepthAttachment);
    vkw::CmdBeginRendering(attachs, ctx.depth);
    vkw::CmdBindPipeline(ctx.opaquePipeline);
}

void EndPass() {
    vkw::CmdEndRendering();
}

void ShadowMapPass(Ref<LightNode>& light, Ref<SceneAsset>& scene, GPUScene& gpuScene) {
    if (scene->shadowType != ShadowType::ShadowMap) {
        return;
    }

    ShadowMapData& shadowMap = gpuScene.GetShadowMap(light->uuid);
    vkw::Image& img = shadowMap.img;
    vkw::CmdBarrier(img, vkw::Layout::DepthAttachment);

    ShadowMapConstants constants;
    constants.modelBufferIndex = gpuScene.GetModelsBuffer();
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.lightIndex = shadowMap.lightIndex;

    uint32_t layers = light->lightType == LightNode::LightType::Point ? 6u : 1u;

    vkw::CmdBeginRendering({}, {img}, layers);
    vkw::CmdBindPipeline(ctx.shadowMapPipeline);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    auto& allModels = gpuScene.GetMeshModels();
    for (GPUModel& model : allModels) {
        constants.modelID = model.modelRID;
        vkw::CmdPushConstants(&constants, sizeof(constants));
        vkw::CmdDrawMesh(model.mesh.vertexBuffer, model.mesh.indexBuffer, model.mesh.indexCount);
    }
    vkw::CmdEndRendering();
    vkw::CmdBarrier(img, vkw::Layout::DepthRead);
    shadowMap.readable = true;
}

void ScreenSpaceVolumetricLightPass(GPUScene& gpuScene) {
    vkw::CmdBarrier(ctx.light, vkw::Layout::General);
    vkw::CmdBindPipeline(ctx.ssvlPipeline);
    VolumetricLightConstants constants;
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.modelBufferIndex = gpuScene.GetModelsBuffer();
    constants.depthRID = ctx.depth.RID();
    constants.lightRID = ctx.light.RID();
    constants.imageSize = {ctx.light.width, ctx.light.height};
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDispatch({ctx.light.width / 32 + 1, ctx.light.height / 32 + 1, 1});
    vkw::CmdBarrier(ctx.light, vkw::Layout::ShaderRead);
}

void ShadowMapVolumetricLightPass(GPUScene& gpuScene) {
    vkw::CmdBarrier(ctx.light, vkw::Layout::General);
    vkw::CmdBindPipeline(ctx.shadowMapVolumetricLightPipeline);
    VolumetricLightConstants constants;
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.modelBufferIndex = gpuScene.GetModelsBuffer();
    constants.depthRID = ctx.depth.RID();
    constants.lightRID = ctx.light.RID();
    constants.imageSize = {ctx.light.width, ctx.light.height};
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDispatch({ctx.light.width / 32 + 1, ctx.light.height / 32 + 1, 1});
    vkw::CmdBarrier(ctx.light, vkw::Layout::ShaderRead);
}

void LightPass(LightConstants constants) {
    std::vector<vkw::Image> attachs = { ctx.albedo, ctx.normal, ctx.material, ctx.emission };
    for (auto& attach : attachs) {
        vkw::CmdBarrier(attach, vkw::Layout::ShaderRead);
    }
    vkw::CmdBarrier(ctx.depth, vkw::Layout::DepthRead);
    vkw::CmdBarrier(ctx.light, vkw::Layout::ColorAttachment);

    constants.albedoRID = ctx.albedo.RID();
    constants.normalRID = ctx.normal.RID();
    constants.materialRID = ctx.material.RID();
    constants.emissionRID = ctx.emission.RID();
    constants.depthRID = ctx.depth.RID();

    vkw::CmdBeginRendering({ ctx.light }, {});
    vkw::CmdBindPipeline(ctx.lightPipeline);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDrawPassThrough();
    vkw::CmdEndRendering();

    vkw::CmdBarrier(ctx.light, vkw::Layout::ShaderRead);
}

void ComposePass(bool separatePass, Output output) {
    ComposeConstant constants;
    constants.lightRID = ctx.light.RID();
    constants.albedoRID = ctx.albedo.RID();
    constants.normalRID = ctx.normal.RID();
    constants.materialRID = ctx.material.RID();
    constants.emissionRID = ctx.emission.RID();
    constants.depthRID = ctx.depth.RID();
    constants.imageType = uint32_t(output);

    if (separatePass) {
        vkw::CmdBarrier(ctx.compose, vkw::Layout::ColorAttachment);
        vkw::CmdBeginRendering({ ctx.compose });
    }
    vkw::CmdBindPipeline(ctx.composePipeline);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDrawPassThrough();
    if (separatePass) {
        vkw::CmdEndRendering();
        vkw::CmdBarrier(ctx.compose, vkw::Layout::ShaderRead);
    }
}

vkw::Image& GetComposedImage() {
    return ctx.compose;
}

void ViewportOnImGui() {
    ImGui::Image(ctx.compose.ImGuiRID(), ImGui::GetWindowSize());

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
}

}