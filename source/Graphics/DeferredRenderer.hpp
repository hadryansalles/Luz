#pragma once

#include "Base.hpp"

namespace vkw {

struct Image;

}

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

void CreateShaders();
void CreateImages(uint32_t width, uint32_t height);
void Destroy();

void LightPass(LightConstants constants);
void ComposePass(bool separatePass);
void BeginOpaquePass();
void EndPass();
void OnImgui(int numFrame);

vkw::Image& GetComposedImage();

}