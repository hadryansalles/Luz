#include "Luzpch.hpp"

#include "CameraController.hpp"
#include "Window.hpp"

#include "AssetManager.hpp"
#include <cmath>

void CameraController::Update(Ref<struct SceneAsset>& scene, Ref<struct CameraNode>& cam, bool viewportHovered, float deltaTime) {
    if (!viewportHovered) {
       return;
    }

    if (Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) && Window::IsKeyDown(GLFW_KEY_1)) {
        cam->mode = CameraNode::CameraMode::Orbit;
        moveSpeed = glm::vec3(.0f);
        rotationSpeed = glm::vec3(.0f);
    }
    if (Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) && Window::IsKeyDown(GLFW_KEY_2)) {
        cam->mode = CameraNode::CameraMode::Fly;
        moveSpeed = glm::vec3(.0f);
        rotationSpeed = glm::vec3(.0f);
    }

    glm::vec2 drag(.0f);
    glm::vec2 move(.0f);
    glm::vec3 fpsMove(.0f, .0f, .0f);

    float dt = fminf(deltaTime, 33.3f);
    float scroll = Window::GetDeltaScroll();
    float slowDown = Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) ? 0.1f : 1.0f;
    float speedUp = 1.0f;

    if (Window::IsMouseDown(GLFW_MOUSE_BUTTON_1) || Window::IsKeyDown(GLFW_KEY_LEFT_ALT)) {
        drag = -Window::GetDeltaMouse();
        drag.x *= -1.0f;
    }
    if (Window::IsMouseDown(GLFW_MOUSE_BUTTON_2) || Window::IsKeyDown(GLFW_KEY_LEFT_SHIFT)) {
        move = -Window::GetDeltaMouse();
    }
    if (Window::IsKeyDown(GLFW_KEY_CAPS_LOCK)) {
        speedUp = 3.0f;
    }
    if (Window::IsKeyDown(GLFW_KEY_W)) {
        fpsMove.z -= 1;
    }
    if (Window::IsKeyDown(GLFW_KEY_S)) {
        fpsMove.z += 1;
    }
    if (Window::IsKeyDown(GLFW_KEY_D)) {
        fpsMove.x += 1;
    }
    if (Window::IsKeyDown(GLFW_KEY_A)) {
        fpsMove.x -= 1;
    }
    if (scroll != 0) {
        fpsMove.y += scroll;
    }

    if (scene->autoOrbit) {
        drag.x = 1*dt;
    }

    glm::mat4 view = cam->GetView();
    glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);
    glm::vec3 front = glm::vec3(view[0][2], view[1][2], view[2][2]);

    float maxMoveSpeed = scene->camSpeed * slowDown * speedUp;
    float maxRotationSpeed = scene->rotationSpeed;
    float zoomSpeed = scene->zoomSpeed;

    glm::vec3 targetMoveSpeed = glm::vec3(.0f);

    glm::vec3 targetRotationSpeed = glm::vec3(drag.y, drag.x, 0) * maxRotationSpeed;
    rotationSpeed = glm::mix(rotationSpeed, targetRotationSpeed, rotationSmooth);
    glm::vec3 newRotation = cam->rotation + rotationSpeed;
    newRotation.x = std::max(-89.9f, std::min(89.9f, newRotation.x));
    cam->rotation = newRotation;

    switch (cam->mode) {
    case CameraNode::CameraMode::Orbit:
        targetMoveSpeed = maxMoveSpeed * glm::vec3(move.x, move.y, -scroll);
        moveSpeed = glm::mix(moveSpeed, targetMoveSpeed, dt / accelerationTime);
        cam->zoom *= (float)std::pow(10, moveSpeed.z * zoomSpeed);
        cam->center -= (right * moveSpeed.x - up * moveSpeed.y);
        break;
    case CameraNode::CameraMode::Fly:
        targetMoveSpeed = maxMoveSpeed * fpsMove;
        moveSpeed = glm::mix(moveSpeed, targetMoveSpeed, dt / accelerationTime);
        cam->eye += (right * moveSpeed.x + front * moveSpeed.z + up * moveSpeed.y) * dt;
        break;
    }
}

/*

void Camera::OnImgui() {
    float totalWidth = ImGui::GetContentRegionAvail().x;
    auto viewDirty = false;
    auto projDirty = false;
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::Checkbox("Auto orbit", &autoOrbit);
        // type
        {
            ImGui::Text("Projection:");
            ImGui::SameLine(totalWidth/3.0f);
            if (ImGui::RadioButton("Perspective", type == Type::Perspective)) {
                type = Type::Perspective;
                projDirty = true;
                viewDirty = true;
            }
            ImGui::SameLine(2*totalWidth/3.0f);
            if (ImGui::RadioButton("Orthographic", type == Type::Orthographic)) {
                type = Type::Orthographic;
                viewDirty = true;
                projDirty = true;
            }
        }
        // control mode
        {
            ImGui::Text("Control:");
            ImGui::SameLine(totalWidth/3.0f);
            if (ImGui::RadioButton("Orbit", mode == Control::Orbit) && mode != Control::Orbit) {
                SetControl(Control::Orbit);
                viewDirty = true;
            }
            ImGui::SameLine(2*totalWidth/3.0f);
            if (ImGui::RadioButton("Fly", mode == Control::Fly) && mode != Control::Fly) {
                SetControl(Control::Fly);
                viewDirty = true;
            }
        }
        // parameters
        {
            switch (type) {
            case Type::Orthographic:
                break;
            case Type::Perspective:
                ImGui::Text("Horizontal Fov");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("fov");
                projDirty |= ImGui::DragFloat("", &horizontalFov, 1, 30, 120);
                ImGui::PopID();
                break;
            }
            ImGui::Text("Near");
            ImGui::SameLine(totalWidth/3.0f);
            ImGui::PushID("near");
            if (type == Type::Orthographic) {
                projDirty |= ImGui::DragFloat("", &orthoNearDistance, 1, -10000000, -1);
            }
            else {
                projDirty |= ImGui::DragFloat("", &nearDistance, 0.001, 0.000001, 10);
            }
            ImGui::PopID();
            ImGui::Text("Far");
            ImGui::SameLine(totalWidth/3.0f);
            ImGui::PushID("far");
            if (type == Type::Orthographic) {
                projDirty |= ImGui::DragFloat("", &orthoFarDistance, 1, 1, 10000000);
            }
            else {
                projDirty |= ImGui::DragFloat("", &farDistance, 1, 1, 10000000);
            }
            ImGui::PopID();
            if (mode == Control::Fly) {
                ImGui::Text("Eye");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("eye");
                viewDirty |= ImGui::DragFloat3("", glm::value_ptr(eye));
                ImGui::PopID();
            }
            else {
                ImGui::Text("Center");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("center");
                viewDirty |= ImGui::DragFloat3("", glm::value_ptr(center));
                ImGui::PopID();
            }
            ImGui::Text("Rotation");
            ImGui::SameLine(totalWidth/3.0f);
            ImGui::PushID("rotation");
            viewDirty |= ImGui::DragFloat3("", glm::value_ptr(rotation));
            ImGui::PopID();
            ImGui::Text("Zoom");
            ImGui::SameLine(totalWidth/3.0f);
            ImGui::PushID("zoom");
            viewDirty |= ImGui::DragFloat("", &zoom, 0.1, 0.0001, 10000);
            ImGui::PopID();
        }
        // static parameters
        {
            if (ImGui::TreeNode("Speeds")) {
                ImGui::Text("Movement");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("mov");
                ImGui::DragFloat("", &speed, 0.0001, 0, 10.0);
                ImGui::PopID();
                ImGui::Text("Rotation");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("rot");
                ImGui::DragFloat("", &rotationSpeed, 0.001, 0, 1);
                ImGui::PopID();
                ImGui::Text("Zoom");
                ImGui::SameLine(totalWidth/3.0f);
                ImGui::PushID("zoom");
                ImGui::DragFloat("", &zoomSpeed, 0.001, 0, 1);
                ImGui::PopID();
                ImGui::TreePop();
            }
        }

        if (viewDirty) {
            UpdateView();
        }

        if (projDirty) {
            UpdateProj();
        }
    }
}

*/
