#pragma once

#include <vulkan/vulkan.h>

#include "RenderingPass.hpp"

struct Light;

namespace DeferredShading {

struct OpaqueConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

struct LightConstants {
    int sceneBufferIndex;
    int frameID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
    int volumetricLightRID;
    int ambientOcclusionRID;
};

struct ShadowConstants {
    int sceneBufferIndex;
    int lightId;
    int frameId;
    int normalRID;
    int depthRID;
};

struct BlurConstants {
    int imageRID;
    int depthRID;
    int normalRID;
    int steps;
    glm::vec2 delta;
};

inline RID panoramaRID;
inline ImageResource faces[6];

inline RenderingPass opaquePass;
inline RenderingPass envmapPass;
inline RenderingPass lightPass;
inline RenderingPass aoPass;
inline RenderingPass panoramaToCubePass;
inline RenderingPass presentPass;
inline RenderingPass shadowPass;
inline RenderingPass volumetricLightPass;
inline RenderingPass rgbaBlurPass;
inline RenderingPass floatBlurPass;

inline std::vector<RenderingPass*> renderingPasses;

void Setup();
void Create();
void Destroy();
void ReloadShaders();

void RenderMesh(VkCommandBuffer commandBuffer, RID meshId);

void Blur4Pass(VkCommandBuffer commandBuffer, RID imageRID, int blurSize);
void Blur1Pass(VkCommandBuffer commandBuffer, RID imageRID, int blurSize);

void AmbientOcclusionPass(VkCommandBuffer commandBuffer, LightConstants constants);

void ShadowPass(VkCommandBuffer commandBuffer, ShadowConstants constants, Light* light);

void VolumetricLightPass(VkCommandBuffer commandBuffer, LightConstants constants);
void LightPass(VkCommandBuffer commandBuffer, LightConstants constants);

void EnvmapPass(VkCommandBuffer commandBuffer, OpaqueConstants constants);

void BindConstants(VkCommandBuffer commandBuffer, RenderingPass& pass, void* data, u32 size);
void BeginOpaquePass(VkCommandBuffer commandBuffer);
void EndPass(VkCommandBuffer commandBuffer);

void BeginPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void EndPresentPass(VkCommandBuffer commandBuffer, int numFrame);
void OnImgui(int numFrame);

void RenderPanoramaToFace(VkCommandBuffer commandBuffer, RID panoramaRID, ImageResource face, int i);

}