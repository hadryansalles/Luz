#pragma once

#include "Transform.hpp"
#include "Descriptors.hpp"

struct PointLight {
    glm::vec4 color    = glm::vec4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    float intensity    = 1;
};

#define MAX_POINT_LIGHTS 10

struct LightsUBO {
    PointLight pointLights[MAX_POINT_LIGHTS];
    glm::vec3 ambientColor = glm::vec3(1.0f);
    u32 numPointLights     = 0;
    f32 ambientIntensity   = 0.1f;
    u32 PADDING[11];
};

struct PointLightUBO {
    glm::vec4 color;
    glm::vec3 position;
    float intensity;
};

struct Collection;

enum LightType {
    Point,
};

struct Light {
    std::string name = "Light";
    Transform transform;
    Collection* collection = nullptr;
    glm::vec4 color = glm::vec4(1.0f);
    float intensity = 1.0f;
    BufferDescriptor descriptor;
    LightType type = LightType::Point;
};

class LightManager {
    static inline const char* TYPE_NAMES[] = { "Point" };
    static inline LightsUBO uniformData;
    static inline BufferResource buffer;
    static inline std::vector<Light*> lights;
    static inline std::vector<bool> dirtyBuffer;
    static inline bool dirtyUniform = false;

    static void PointLightOnImgui(Light* light);
    static void UpdateUniformIfDirty();

public:
    static void Create();
    static void Destroy();
    static void Finish();
    static void OnImgui();
    static void OnImgui(Light* light);

    static void UpdateBufferIfDirty(int numFrame);

    static Light* CreateLight(LightType type = LightType::Point);
    static void DestroyLight(Light* light);

    static void SetDirty();
    static void SetAmbient(glm::vec3 color, float intensity);
};