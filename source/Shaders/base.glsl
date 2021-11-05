#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform ConstantsBlock{
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

#define SCENE_BUFFER_INDEX 0
#define MODEL_BUFFER_INDEX 1

#define LIGHT_BUFFER_INDEX 2
#define lights lightsBuffers[numFrames*LIGHT_BUFFER_INDEX + frameID]

#define MAX_LIGHTS_PER_TYPE 8
#define MAX_MODELS 4096

struct LightBlock {
    vec3 color;
    float intensity;
    vec3 position;
    float innerAngle;
    vec3 direction;
    float outerAngle;
};

struct ModelBlock {
    mat4 modelMat;
    vec4 colors[2];
    float values[2];
    int textures[2];
};

layout(set = 0, binding = 1) readonly buffer SceneBlock {
    LightBlock pointLights[MAX_LIGHTS_PER_TYPE];
    LightBlock spotLights[MAX_LIGHTS_PER_TYPE];
    LightBlock dirLights[MAX_LIGHTS_PER_TYPE];
    vec3 ambientLightColor;
    float ambientLightIntensity;
    mat4 projView;
    vec3 camPos;
    int numPointLights;
    int numSpotLights;
    int numDirLights;
} sceneBuffers[];

layout(set = 0, binding = 1) readonly buffer ModelBuffer {
    ModelBlock models[MAX_MODELS];
} modelsBuffers[];

#define scene sceneBuffers[sceneBufferIndex]
#define model modelsBuffers[modelBufferIndex].models[modelID]
