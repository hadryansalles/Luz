#pragma once

#include <glm/glm.hpp>

struct CameraController {
    float accelerationTime = 200.0f;
    float rotationSmooth = 0.2f;

    glm::vec3 moveSpeed = glm::vec3(.0f);
    glm::vec3 rotationSpeed = glm::vec3(.0f);

    void Update(Ref<struct SceneAsset>& scene, Ref<struct CameraNode>& cam, bool viewportHovered, float deltaTime);
};
