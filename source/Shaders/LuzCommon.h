#if defined(LUZ_ENGINE)
#pragma once
using vec2 = glm::vec2;
using ivec2 = glm::ivec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
#endif

#define LUZ_PI 3.14159265359
#define PI 3.14159265359
#define GOLDEN_RATIO 2.118033988749895
#define LUZ_EPS 0.000001f

#define LUZ_BINDING_TEXTURE 0
#define LUZ_BINDING_BUFFER 1
#define LUZ_BINDING_TLAS 2
#define LUZ_BINDING_STORAGE_IMAGE 3

#define LUZ_MAX_LIGHTS 64
#define LUZ_MAX_MODELS 8192
#define LUZ_MAX_LINES 2048

#define LUZ_LIGHT_TYPE_POINT 0
#define LUZ_LIGHT_TYPE_SPOT 1
#define LUZ_LIGHT_TYPE_DIRECTIONAL 2

#define SHADOW_TYPE_RAYTRACING 1
#define SHADOW_TYPE_MAP 2

#define VOLUMETRIC_TYPE_SCREEN_SPACE 1
#define VOLUMETRIC_TYPE_SHADOW_MAP 2

#define LUZ_HISTOGRAM_THREADS 16
#define LUZ_HISTOGRAM_BINS 256

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

    int nodeId[2];
    int pad[2];
};

struct SceneBlock {
    LightBlock lights[LUZ_MAX_LIGHTS];

    vec3 ambientLightColor;
    float ambientLightIntensity;

    mat4 proj;
    mat4 view;
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 inverseProj;
    mat4 inverseView;

    vec2 jitter;
    vec2 prevJitter;

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
    int pcfSamples;
    int pad[2];
};

struct OpaqueConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int frame;

    vec2 mousePos;
    int mousePickingBufferIndex;
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

struct LightConstants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int frame;
    int albedoRID;

    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;

    int atmosphericScatteringRID;
    int atmosphericTransmittanceRID;
    int pad[2];
};

struct ComposeConstants {
    int imageType;
    int lightRID;
    int albedoRID;
    int normalRID;

    int materialRID;
    int emissionRID;
    int depthRID;
    int debugRID;

    float exposure;
    float pad[3];
};

struct PostProcessingConstants {
    int lightInputRID;
    int lightOutputRID;
    int lightHistoryRID;
    int depthRID;

    vec2 size;
    int sceneBufferIndex;
    int reconstruct;

    float histogramMinLog;
    float deltaTime;
    int histogramRID;
    int histogramAverageRID;

    float histogramOneOverLog;
    int frame;
    int pad[2];
};

struct AtmosphericConstants {
    int sceneBufferIndex;
    int transmittanceRID;
    int scatteringRID;
    int frame;

    ivec2 imageSize;
    int pad[2];
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

layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform sampler2D textures[];
layout(set = 0, binding = LUZ_BINDING_TEXTURE) uniform samplerCube cubeTextures[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) readonly buffer SceneBuffer {
    SceneBlock block;
} sceneBuffers[];

layout(set = 0, binding = LUZ_BINDING_BUFFER) buffer MousePickingBuffer {
    int data[];
} mousePickingBuffers[];

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

#define scene sceneBuffers[ctx.sceneBufferIndex].block
#define tlas tlasBuffer[scene.tlasRid]
#define model modelsBuffers[ctx.modelBufferIndex].models[ctx.modelID]
#define vertexBuffer vertexBuffers[model.vertexBuffer]
#define lineBlocks lineBuffers[ctx.linesRID].data

#endif