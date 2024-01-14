// only defined when included from c++ code

#define SCENE_BUFFER_INDEX 0
#define MODELS_BUFFER_INDEX 1

#ifdef LUZ_ENGINE
#pragma once

#include <glm/glm.hpp>

using float3 = glm::vec3;
using float4 = glm::vec4;
using mat4 = glm::mat4;
using int2 = glm::ivec2;

#endif

struct SceneBlock2 {
    float3 ambientLightColor;
    float ambientLightIntensity;
    mat4 projView;
    mat4 inverseProj;
    mat4 inverseView;
    float3 camPos;
    uint32_t numLights;
    uint32_t aoNumSamples;
    float aoMin;
    float aoMax;
    float aoPower;
    int2 viewSize;
    uint32_t useBlueNoise;
};

struct MaterialBlock2 {
    float4 color;
    float3 emission;
    float metallic;
    float roughness;
    uint32_t aoMap;
    uint32_t colorMap;
    uint32_t normalMap;
    uint32_t emissionMap;
    uint32_t metallicRoughnessMap;
    uint32_t PADDING[2];
};

struct ModelBlock2 {
    mat4 model;
    MaterialBlock2 material;
};