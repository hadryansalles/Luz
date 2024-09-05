#if defined(LUZ_ENGINE)
#pragma once
using vec2 = glm::vec2;
using ivec2 = glm::ivec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
#endif

#define LUZ_BINDING_TEXTURE 0
#define LUZ_BINDING_BUFFER 1
#define LUZ_BINDING_TLAS 2
#define LUZ_BINDING_STORAGE_IMAGE 3

#define LUZ_MAX_LIGHTS 64
#define LUZ_MAX_MODELS 8192
#define LUZ_MAX_LINES 1024

#define LUZ_LIGHT_TYPE_POINT 0
#define LUZ_LIGHT_TYPE_SPOT 1
#define LUZ_LIGHT_TYPE_DIRECTIONAL 2

#define SHADOW_TYPE_RAYTRACING 1
#define SHADOW_TYPE_MAP 2

#define VOLUMETRIC_TYPE_SCREEN_SPACE 1
#define VOLUMETRIC_TYPE_SHADOW_MAP 2

struct LightBlock {
    vec3 color;
    float intensity;

    vec3 position;
    float angle;  // Changed from innerAngle

    vec3 direction;
    float blendFactor;  // Changed from outerAngle

    int type;
    int numShadowSamples;
    float radius;
    int shadowMap;

    mat4 viewProj[6];
    float zFar;
    int volumetricType;
    float volumetricWeight;
    float volumetricAbsorption;

    float volumetricDensity;
    int volumetricSamples;
    int pad[2];
};

struct LineBlock {
    vec4 color;

    vec3 p0;
    int depthAware;

    vec3 p1;
    int isPoint;

    float thickness;
    float pad[3];
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
    int vertexBuffer;
    int indexBuffer;
};

struct SceneBlock {
    LightBlock lights[LUZ_MAX_LIGHTS];

    vec3 ambientLightColor;
    float ambientLightIntensity;

    mat4 proj;
    mat4 view;
    mat4 projView;
    mat4 prevProjView;
    mat4 inverseProj;
    mat4 inverseView;

    vec3 camPos;
    int numLights;

    float aoMin;
    float aoMax;
    float exposure;
    int aoNumSamples;

    int whiteTexture;
    int blackTexture;
    int blueNoiseTexture;
    int tlasRid;

    int shadowType;
};

struct ShadowMapConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int lightIndex;
};

struct VolumetricLightConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int depthRID;
    int lightRID;
    ivec2 imageSize;
    int frame;
    int pad[1];
};

struct LineRenderingConstants {
    ivec2 imageSize;
    int linesRID;
    int sceneBufferIndex;

    int depthRID;
    int outputRID;
    int lineCount;
    int depthAware;

    vec4 color;
};

struct FontRenderingConstants {
    vec4 color;

    vec3 pos;
    float pad;

    int charCode;
    int charSize;
    int charIndex;
    int sceneBufferIndex;

    float fontSize;
    int depthAware;
    int depthRID;
    int fontRID;
};

struct PostProcessingConstants {
    int lightRID;
    int lightDestRID;
    int normalRID;
    int historyRID;
    int depthRID;
    int sceneBufferIndex;
    int filterSize;
};

#if !defined(LUZ_ENGINE)

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable
#extension GL_EXT_shader_image_load_formatted : require

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 2
#define LIGHT_TYPE_SPOT 1

#define PI 3.14159265359
#define GOLDEN_RATIO 2.118033988749895

layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform sampler2D textures[];
layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform samplerCube cubeTextures[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer SceneBuffer {
    SceneBlock block;
} sceneBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer LineBuffer {
    LineBlock data[];
} lineBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer ModelBuffer {
    ModelBlock models[LUZ_MAX_MODELS];
} modelsBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer VertexBuffer {
    float data[];
} vertexBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffers[];

layout(set = 0, binding = LUZ_BINDING_TLAS) uniform accelerationStructureEXT tlasBuffer[];
layout(binding = LUZ_BINDING_STORAGE_IMAGE) uniform image2D images[];

#define tlas tlasBuffer[scene.tlasRid]
#define scene sceneBuffers[sceneBufferIndex].block
#define scene2 sceneBuffers[ctx.sceneBufferIndex].block
#define model modelsBuffers[modelBufferIndex].models[modelID]
#define vertexBuffer vertexBuffers[model.vertexBuffer]
#define sceneBlock sceneBuffers[ctx.sceneBufferIndex].block
#define lineBlocks lineBuffers[ctx.linesRID].data

#endif