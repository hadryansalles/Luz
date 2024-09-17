#include "Luzpch.hpp"

#include "GPUScene.hpp"
#include "AssetManager.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanWrapper.h"
#include "LuzCommon.h"
#include "DebugDraw.h"

#include "FileManager.hpp"
#include <imgui/ImGuizmo.h>

namespace DeferredRenderer {

struct Context {
    vkw::Pipeline opaquePipeline;
    vkw::Pipeline lightPipeline;
    vkw::Pipeline composePipeline;
    vkw::Pipeline shadowMapPipeline;
    vkw::Pipeline ssvlPipeline;
    vkw::Pipeline shadowMapVolumetricLightPipeline;
    vkw::Pipeline lineRenderingPipeline;
    vkw::Pipeline fontRenderingPipeline;
    vkw::Pipeline postProcessingPipeline;

    std::unordered_map<std::string, int> shaderVersions;

    vkw::Image albedo;
    vkw::Image normal;
    vkw::Image material;
    vkw::Image emission;
    vkw::Image depth;
    vkw::Image lightA;
    vkw::Image lightB;
    vkw::Image lightHistory;
    vkw::Image compose;
    vkw::Image debug;
};

Context ctx;

void CreatePipeline(vkw::Pipeline& pipeline, const vkw::PipelineDesc& desc) {
    bool should_update = false;
    for (auto& stage : desc.stages) {
        auto path = "source/Shaders/" + stage.path.string();
        auto it = ctx.shaderVersions.find(path);
        auto version = FileManager::GetFileVersion(path);
        if (it == ctx.shaderVersions.end() || version > it->second) {
            ctx.shaderVersions[path] = version;
            should_update = true;
        }
    }
    if (should_update) {
        pipeline = vkw::CreatePipeline(desc);
    }
}

void CreateShaders() {
    if (ctx.albedo.format == 0) {
        LOG_CRITICAL("CREATE IMAGES BEFORE SHADERS IN DEFERRED RENDERER");
    }
    CreatePipeline(ctx.lightPipeline, {
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "light.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "light.frag"},
        },
        .name = "Light Pipeline",
        .vertexAttributes = {},
        .colorFormats = {ctx.lightA.format},
        .useDepth = false,
    });
    CreatePipeline(ctx.opaquePipeline, {
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
    CreatePipeline(ctx.shadowMapPipeline, {
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
    CreatePipeline(ctx.composePipeline, {
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
    CreatePipeline(ctx.ssvlPipeline, {
        .point = vkw::PipelinePoint::Compute,
        .stages = {
            {.stage = vkw::ShaderStage::Compute, .path = "screenSpaceVolumetricLight.comp"},
        },
        .name = "VolumetricLight Pipeline",
    });
    CreatePipeline(ctx.shadowMapVolumetricLightPipeline, {
        .point = vkw::PipelinePoint::Compute,
        .stages = {
            {.stage = vkw::ShaderStage::Compute, .path = "shadowMapVolumetricLight.comp"},
        },
        .name = "ShadowMapVolumetricLight Pipeline",
    });
    CreatePipeline(ctx.lineRenderingPipeline, {
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "debug.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "debug.frag"},
        },
        .name = "LineRendering Pipeline",
        .vertexAttributes = {vkw::Format::RGB32_sfloat},
        .colorFormats = {ctx.debug.format},
        .useDepth = false,
        .lineTopology = true,
    });
    CreatePipeline(ctx.fontRenderingPipeline, {
        .point = vkw::PipelinePoint::Graphics,
        .stages = {
            {.stage = vkw::ShaderStage::Vertex, .path = "font.vert"},
            {.stage = vkw::ShaderStage::Fragment, .path = "font.frag"},
        },
        .name = "FontRendering Pipeline",
        .vertexAttributes = {vkw::Format::RGB32_sfloat},
        .colorFormats = {ctx.debug.format},
        .useDepth = false,
    });
    CreatePipeline(ctx.postProcessingPipeline, {
        .point = vkw::PipelinePoint::Compute,
        .stages = {
            {.stage = vkw::ShaderStage::Compute, .path = "taa.comp"},
        },
        .name = "PostProcessing Pipeline",
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
    ctx.debug = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
        .name = "Debug Attachment"
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
    ctx.lightA = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA32_sfloat,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled | vkw::ImageUsage::Storage,
        .name = "Light Attachment A"
    });
    ctx.lightB = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA32_sfloat,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled | vkw::ImageUsage::Storage,
        .name = "Light Attachment B"
    });
    ctx.lightHistory = vkw::CreateImage({
        .width = width,
        .height = height,
        .format = vkw::Format::RGBA32_sfloat,
        .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled | vkw::ImageUsage::Storage,
        .name = "Light History Attachment"
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

void ScreenSpaceVolumetricLightPass(GPUScene& gpuScene, int frame) {
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::General);
    vkw::CmdBindPipeline(ctx.ssvlPipeline);
    VolumetricLightConstants constants;
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.modelBufferIndex = gpuScene.GetModelsBuffer();
    constants.depthRID = ctx.depth.RID();
    constants.lightRID = ctx.lightA.RID();
    constants.imageSize = {ctx.lightA.width, ctx.lightA.height};
    constants.frame = frame;
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDispatch({ctx.lightA.width / 32 + 1, ctx.lightA.height / 32 + 1, 1});
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::ShaderRead);
}

void ShadowMapVolumetricLightPass(GPUScene& gpuScene, int frame) {
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::General);
    vkw::CmdBindPipeline(ctx.shadowMapVolumetricLightPipeline);
    VolumetricLightConstants constants;
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.modelBufferIndex = gpuScene.GetModelsBuffer();
    constants.depthRID = ctx.depth.RID();
    constants.lightRID = ctx.lightA.RID();
    constants.imageSize = {ctx.lightA.width, ctx.lightA.height};
    constants.frame = frame;
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDispatch({ctx.lightA.width / 32 + 1, ctx.lightA.height / 32 + 1, 1});
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::ShaderRead);
}

void LightPass(LightConstants constants) {
    std::vector<vkw::Image> attachs = { ctx.albedo, ctx.normal, ctx.material, ctx.emission };
    for (auto& attach : attachs) {
        vkw::CmdBarrier(attach, vkw::Layout::ShaderRead);
    }
    vkw::CmdBarrier(ctx.depth, vkw::Layout::DepthRead);
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::ColorAttachment);

    constants.albedoRID = ctx.albedo.RID();
    constants.normalRID = ctx.normal.RID();
    constants.materialRID = ctx.material.RID();
    constants.emissionRID = ctx.emission.RID();
    constants.depthRID = ctx.depth.RID();

    vkw::CmdBeginRendering({ ctx.lightA }, {});
    vkw::CmdBindPipeline(ctx.lightPipeline);
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDrawPassThrough();
    vkw::CmdEndRendering();

    vkw::CmdBarrier(ctx.lightA, vkw::Layout::ShaderRead);
}

void ComposePass(bool separatePass, Output output) {
    ComposeConstants constants;
    constants.lightRID = ctx.lightA.RID();
    constants.albedoRID = ctx.albedo.RID();
    constants.normalRID = ctx.normal.RID();
    constants.materialRID = ctx.material.RID();
    constants.emissionRID = ctx.emission.RID();
    constants.depthRID = ctx.depth.RID();
    constants.imageType = uint32_t(output);
    constants.debugRID = ctx.debug.RID();

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

void LineRenderingPass(GPUScene& gpuScene) {
    auto strips = DebugDraw::Get();
    if (strips.size() == 0) {
        return;
    }
    vkw::CmdBarrier(ctx.debug, vkw::Layout::ColorAttachment);
    vkw::CmdBeginRendering({ ctx.debug });
    vkw::CmdBindPipeline(ctx.lineRenderingPipeline);

    LineRenderingConstants constants;
    constants.imageSize = {ctx.lightA.width, ctx.lightA.height};
    constants.linesRID = 0;
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    constants.depthRID = ctx.depth.RID();
    constants.outputRID = ctx.lightA.RID();
    constants.lineCount = 0;

    int offset = 0;
    for (const auto& s : strips) {
        if (!s.config.hide) {
            constants.color = s.config.color;
            constants.depthAware = s.config.depthAware;
            vkw::CmdPushConstants(&constants, sizeof(constants));
            vkw::CmdDrawLineStrip(gpuScene.GetLinesBuffer(), offset, s.points.size(), s.config.thickness);
        }
        offset += s.points.size();
    }

    FontRenderingConstants fontCtx;
    fontCtx.depthRID = ctx.depth.RID();
    fontCtx.sceneBufferIndex = gpuScene.GetSceneBuffer();
    vkw::CmdBindPipeline(ctx.fontRenderingPipeline);
    for (const auto& s : strips) {
        if (!s.config.hide && s.config.drawTitle && s.points.size() > 0) {
            fontCtx.color = s.config.color;
            fontCtx.depthAware = s.config.depthAware;
            fontCtx.fontRID = gpuScene.GetFontBitmap();
            fontCtx.fontSize = 0.5f;
            fontCtx.charSize = 64;
            fontCtx.pos = s.points[0];
            for (int i = 0; i < s.name.size(); i++) {
                fontCtx.charCode = (uint8_t)s.name.c_str()[i];
                fontCtx.charIndex = i;
                vkw::CmdPushConstants(&fontCtx, sizeof(fontCtx));
                vkw::CmdDrawPassThrough();
            }
        }
    }

    vkw::CmdEndRendering();
    vkw::CmdBarrier(ctx.compose, vkw::Layout::ShaderRead);
}

void TAAPass(GPUScene& gpuScene) {
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::General);
    vkw::CmdBarrier(ctx.lightHistory, vkw::Layout::General);
    vkw::CmdBindPipeline(ctx.postProcessingPipeline);
    PostProcessingConstants constants;
    constants.lightInputRID = ctx.lightA.RID();
    constants.lightOutputRID = ctx.lightB.RID();
    constants.lightHistoryRID = ctx.lightHistory.RID();
    constants.depthRID = ctx.depth.RID();
    constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
    vkw::CmdPushConstants(&constants, sizeof(constants));
    vkw::CmdDispatch({ctx.lightA.width / 32 + 1, ctx.lightA.height / 32 + 1, 1});
    vkw::CmdBarrier(ctx.lightA, vkw::Layout::ShaderRead);
    std::swap(ctx.lightA, ctx.lightB);
}

void SwapLightHistory() {
    std::swap(ctx.lightA, ctx.lightHistory);
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