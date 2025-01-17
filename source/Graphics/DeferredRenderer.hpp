#pragma once

#include "Base.hpp"

namespace vkw {

struct Image;
struct Buffer;

}

struct LightNode;
struct SceneAsset;
struct GPUScene;

namespace DeferredRenderer {

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
void LightPass(GPUScene& gpuScene, int frame);
void ComposePass(bool separatePass, Output output, Ref<SceneAsset>& scene);
void LineRenderingPass(GPUScene& gpuScene);
void BeginOpaquePass();
void EndPass();
void PostProcessingPass(GPUScene& gpuScene);
void TAAPass(GPUScene& gpuScene, Ref<SceneAsset>& scene);
void LuminanceHistogramPass();
void SwapLightHistory();
void AtmosphericPass(GPUScene& gpuScene, int frame);

vkw::Buffer& GetMousePickingBuffer();
vkw::Image& GetComposedImage();

}