#if defined(LUZ_ENGINE)
#pragma once
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
#endif

#define LUZ_BINDING_TEXTURE 0
#define LUZ_BINDING_BUFFER 1
#define LUZ_BINDING_TLAS 2

#define LUZ_MAX_LIGHTS 16
#define LUZ_MAX_MODELS 4096

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
