#pragma once

#include "Base.hpp"

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
};

void Setup();
void Recreate(uint32_t width, uint32_t height);
void Destroy();
void ReloadShaders();

void RenderMesh(RID meshId);
void LightPass(LightConstants constants);
void BeginOpaquePass();
void EndPass();
void BeginPresentPass();
void EndPresentPass();
void OnImgui(int numFrame);

}