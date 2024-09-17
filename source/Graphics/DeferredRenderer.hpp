#pragma once

#include "Base.hpp"

namespace vkw {

struct Image;

}

struct LightNode;
struct SceneAsset;
struct GPUScene;

namespace DeferredRenderer {

struct LightConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int frameID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
};

enum Output : uint32_t {
    Light,
    Albedo,
    Normal,
    Material,
    Emission,
    Depth,
    All,
    Count
};

void CreateShaders();
void CreateImages(uint32_t width, uint32_t height);
void Destroy();

void ShadowMapVolumetricLightPass(GPUScene& gpuScene, int frame);
void ScreenSpaceVolumetricLightPass(GPUScene& gpuScene, int frame);
void ShadowMapPass(Ref<LightNode>& light, Ref<SceneAsset>& scene, GPUScene& gpuScene);
void LightPass(LightConstants constants);
void ComposePass(bool separatePass, Output output);
void LineRenderingPass(GPUScene& gpuScene);
void BeginOpaquePass();
void EndPass();
void PostProcessingPass(GPUScene& gpuScene);
void TAAPass(GPUScene& gpuScene);
void SwapLightHistory();

vkw::Image& GetComposedImage();

}