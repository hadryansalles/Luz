#pragma once

#include "Transform.hpp"
#include "Descriptors.hpp"

struct DirectionalLight {
    glm::vec4 color     = glm::vec4(1.0f);
    glm::vec3 direction = glm::vec3(0.0f);
    float intensity     = 1;
};

struct PointLight {
    glm::vec4 color    = glm::vec4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    float intensity    = 1;
};

#define MAX_POINT_LIGHTS 10
#define MAX_DIRECTIONAL_LIGHTS 10

struct LightsUBO {
    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    glm::vec3 ambientColor   = glm::vec3(1.0f);
    f32 ambientIntensity     = 0.1f;
    u32 numPointLights       = 0;
    u32 numDirectionalLights = 0;
};

struct PointLightUBO {
    glm::vec4 color;
    glm::vec3 position;
    float intensity;
};

struct Collection;

enum LightType {
    Point,
    Directional,
};

struct Light {
    std::string name = "Light";
    Transform transform;
    Collection* collection = nullptr;
    glm::vec4 color = glm::vec4(1.0f);
    float intensity = 1.0f;
    LightType type = LightType::Point;
    int id = -1;
    struct Model* model = nullptr;
};

class LightManager {
    static inline const char* TYPE_NAMES[] = { "Point", "Directional"};
    static inline LightsUBO uniformData;
    static inline BufferResource buffer;
    static inline u64 sectionSize;
    static inline std::vector<Light*> lights;
    static inline std::vector<bool> dirtyBuffer;
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

    static inline std::vector<Light*>& GetLights() { return lights;       }
    static inline bool GetRenderGizmos()           { return renderGizmos; }
};