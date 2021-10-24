#pragma once

#include "Transform.hpp"
#include "Descriptors.hpp"

struct PointLightUBO {
    glm::vec4 color;
    glm::vec3 position;
    float intensity;
};

struct Collection;

enum class LightType {
    PointLight,
};

struct Light {
    std::string name = "Light";
    Transform transform;
    Collection* collection = nullptr;
    glm::vec4 color = glm::vec4(1.0f);
    float intensity = 1.0f;
    BufferDescriptor descriptor;
    LightType type = LightType::PointLight;
    unsigned int id;

    PointLightUBO GetPointLightUBO() {
        PointLightUBO ubo;
        ubo.color = color;
        ubo.intensity = intensity;
        ubo.position = transform.position;
        return ubo;
    }
};

class LightManager {
private:
    static void PointLightOnImgui(Light* light);
    static std::string GetTypeName(LightType type);
public:
    static void OnImgui(Light* light);
};