#pragma once

#include <glm/glm.hpp>

class Camera {
private:
    enum class Control {
        Orbit,
        Fly
    };

    enum class Type {
        Perspective,
        Orthographic
    };

    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 eye;
    glm::vec3 center      = glm::vec3(0);
    glm::vec3 rotation    = glm::vec3(0);
    glm::vec2 extent      = glm::vec2(-1);

    float zoom              = 10.0f;

    float farDistance       = 1000.0f;
    float nearDistance      = 0.01f;
    float horizontalFov     = 60.0f;

    float orthoFarDistance  = 10.0f;
    float orthoNearDistance = -100.0f;

    Type    type          = Type::Perspective;
    Control mode          = Control::Orbit;

    static inline float speed = 0.01;
    static inline float zoomSpeed = 0.1;
    static inline float rotationSpeed = 0.3;

    void UpdateView();
    void UpdateProj();

    void SetControl(Control newMode);

public:
    Camera();
    void Update();
    void OnImgui();
    void SetExtent(float width, float height);

    inline const glm::mat4& GetView() { return view; }
    inline const glm::mat4& GetProj() { return proj; }
};