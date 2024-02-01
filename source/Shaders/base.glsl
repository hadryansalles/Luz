#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#include "common.h"

#define MAX_LIGHTS 16
#define MAX_MODELS 4096
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_SPOT 2

#define PI 3.14159265359
#define GOLDEN_RATIO 2.118033988749895

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
};

layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform sampler2D textures[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer SceneBlock {
    LightBlock lights[MAX_LIGHTS];
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
} sceneBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer ModelBuffer {
    ModelBlock models[MAX_MODELS];
} modelsBuffers[];

layout(set = 0, binding = LUZ_BINDING_TLAS) uniform accelerationStructureEXT tlasBuffer[];

#define tlas tlasBuffer[scene.tlasRid]
#define scene sceneBuffers[sceneBufferIndex]
#define model modelsBuffers[modelBufferIndex].models[modelID]
#define GET_MODEL(id) modelsBuffer[modelBufferIndex].models[(id)]
