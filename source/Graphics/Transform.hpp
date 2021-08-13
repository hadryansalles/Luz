#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct Transform {
    glm::vec3 position = glm::vec3(.0f);
    glm::vec3 rotation = glm::vec3(.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getMatrix() {
        glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rotation)));
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 scaleMat = glm::scale(scale);
        return translationMat * scaleMat * rotationMat;
    }
};
