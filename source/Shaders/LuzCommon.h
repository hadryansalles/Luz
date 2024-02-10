#if defined(LUZ_ENGINE)
#pragma once
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
#endif

#define LUZ_BINDING_TEXTURE 0
#define LUZ_BINDING_BUFFER 1
#define LUZ_BINDING_TLAS 2

#define LUZ_MAX_LIGHTS 64
#define LUZ_MAX_MODELS 4096

#define LUZ_LIGHT_TYPE_POINT 0
#define LUZ_LIGHT_TYPE_SPOT 1
#define LUZ_LIGHT_TYPE_DIRECTIONAL 2

struct LightBlock {
    vec3 color;
    float intensity;

    vec3 position;
    float innerAngle;

    vec3 direction;
    float outerAngle;

    int type;
    int numShadowSamples;
    float radius;
    float pad;
};

struct ModelBlock {
    mat4 modelMat;
    vec4 color;

    vec3 emission;
    float metallic;

    float roughness;
    int aoMap;
    int colorMap;
    int normalMap;

    int emissionMap;
    int metallicRoughnessMap;
    int pad[2];
};

struct SceneBlock {
    LightBlock lights[LUZ_MAX_LIGHTS];

    vec3 ambientLightColor;
    float ambientLightIntensity;

    mat4 projView;
    mat4 inverseProj;
    mat4 inverseView;

    vec3 camPos;
    int numLights;

    float aoMin;
    float aoMax;
    float aoPower;
    int aoNumSamples;

    int useBlueNoise;
    int whiteTexture;
    int blackTexture;
    int tlasRid;
};

#if !defined(LUZ_ENGINE)

#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 2
#define LIGHT_TYPE_SPOT 1

#define PI 3.14159265359
#define GOLDEN_RATIO 2.118033988749895

layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform sampler2D textures[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer SceneBuffer {
    SceneBlock block;
} sceneBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer ModelBuffer {
    ModelBlock models[LUZ_MAX_MODELS];
} modelsBuffers[];

layout(set = 0, binding = LUZ_BINDING_TLAS) uniform accelerationStructureEXT tlasBuffer[];

#define tlas tlasBuffer[scene.tlasRid]
#define scene sceneBuffers[sceneBufferIndex].block
#define model modelsBuffers[modelBufferIndex].models[modelID]

#endif