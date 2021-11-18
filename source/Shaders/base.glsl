#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform ConstantsBlock{
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

#define SCENE_BUFFER_INDEX 0
#define MODEL_BUFFER_INDEX 1

#define MAX_LIGHTS 16
#define MAX_MODELS 4096
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_SPOT 2

#define PI 3.14159265359

struct LightBlock {
    vec3 color;
    float intensity;
    vec3 position;
    float innerAngle;
    vec3 direction;
    float outerAngle;
    int type;
};

struct ModelBlock {
    mat4 modelMat;
    vec3 color;
    float specular;
    float emission;
    float opacity;
    float metallic;
    float roughness;
    int colorMap;
    int normalMap;
    int metallicMap;
    int roughnessMap;
};

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(set = 0, binding = 1) readonly buffer SceneBlock {
    LightBlock lights[MAX_LIGHTS];
    vec3 ambientLightColor;
    float ambientLightIntensity;
    mat4 projView;
    vec3 camPos;
    int numLights;
} sceneBuffers[];

layout(set = 0, binding = 1) readonly buffer ModelBuffer {
    ModelBlock models[MAX_MODELS];
} modelsBuffers[];

#define scene sceneBuffers[sceneBufferIndex]
#define model modelsBuffers[modelBufferIndex].models[modelID]
