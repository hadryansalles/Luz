#pragma once

#include "Transform.hpp"
#include "Descriptors.hpp"

struct PointLight {
    glm::vec4 color    = glm::vec4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    float intensity    = 1;
};

struct DirectionalLight {
    glm::vec4 color     = glm::vec4(1.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f);
    float intensity     = 1;
};

struct SpotLight {
    glm::vec3 color     = glm::vec4(1.0f);
    f32 intensity       = 1;
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f);
    f32 cutOff          = 60.0f;
    glm::vec3 position  = glm::vec3(0.0f);
    f32 outerCutOff     = 90.0f;
};

#define MAX_POINT_LIGHTS 10
#define MAX_DIRECTIONAL_LIGHTS 10
#define MAX_SPOT_LIGHTS 10

struct LightsUBO {
    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    glm::vec3 ambientColor   = glm::vec3(1.0f);
    f32 ambientIntensity     = 0.1f;
    u32 numPointLights       = 0;
    u32 numDirectionalLights = 0;
    u32 numSpotLights        = 0;
};

struct Collection;

enum LightType {
    Point,
    Directional,
    Spot
};

struct Light {
    std::string name = "Light";
    Transform transform;
    Collection* collection = nullptr;
    glm::vec4 color = glm::vec4(1.0f);
    f32 intensity = 1.0f;
    LightType type = LightType::Point;
    f32 cutOff = 60.0f;
    f32 outerCutOff = 90.0f;
    int id = -1;
    struct Model* model = nullptr;
};

class LightManager {
    static inline const char* TYPE_NAMES[] = { "Point", "Directional", "Spot"};
    static inline LightsUBO uniformData;
    static inline UniformBuffer uniformBuffer;
    static inline std::vector<Light*> lights;
    static inline bool dirtyUniform = false;
    static inline struct MeshResource* lightMeshes[3] = { nullptr, nullptr, nullptr };
    static inline float gizmoOpacity = 0.5f;
    static inline bool renderGizmos = true;

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
    static Light* CreateLightCopy(Light* copy);
    static void DestroyLight(Light* light);

    static void SetDirty();
    static void SetAmbient(glm::vec3 color, float intensity);

    static inline glm::vec3 GetAmbientLightColor() { return uniformData.ambientColor; }
    static inline float GetAmbientLightIntensity() { return uniformData.ambientIntensity; }

    static inline std::vector<Light*>& GetLights() { return lights;       }
    static inline bool GetRenderGizmos()           { return renderGizmos; }
};