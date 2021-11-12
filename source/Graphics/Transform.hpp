#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct Transform {
    glm::vec3 position = glm::vec3(.0f);
    glm::vec3 rotation = glm::vec3(.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 transform = glm::mat4(1.0f);
    bool dirty = true;
    Transform* parent = nullptr;

    void SetPosition(glm::vec3 pos) {
        position = pos;
        dirty = true;
    }

    void SetRotation(glm::vec3 rot) {
        rotation = rot;
        dirty = true;
    }

    void SetScale(glm::vec3 scl) {
        scale = scl;
        dirty = true;
    }

    void SetMatrix(glm::mat4 mat) {
        glm::quat rot;
        glm::vec3 skew;
        glm::vec4 persp;
        glm::decompose(mat, scale, rot, position, skew, persp);
        rotation = glm::degrees(glm::eulerAngles(rot));
        dirty = true;
    }

    glm::mat4 GetMatrix() {
        if (dirty) {
            dirty = false;
            glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rotation)));
            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 scaleMat = glm::scale(scale);
            transform = translationMat * scaleMat * rotationMat;
        }
        if (parent) {
            return parent->GetMatrix() * transform;
        }
        return transform;
    }

    glm::vec3 GetGlobalFront() {
        glm::mat4 mat = GetMatrix();
        return glm::normalize(glm::vec3(mat[2][0], mat[2][1], mat[2][2]));
    }

    glm::vec3 GetWorldRotation() {
        return glm::degrees(glm::eulerAngles(glm::quat(GetMatrix())));
    }
};
