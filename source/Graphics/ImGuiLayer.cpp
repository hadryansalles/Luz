#include "ImGuiLayer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

void ImGuiLayer::OnNode(Ref<Node>& node) {
    ImGui::PushID(node->uuid);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.size() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (FindSelected(node) != -1) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    bool open = ImGui::TreeNodeEx(node->name.c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        Select(node);
    }
    if (open) {
        for (auto& child : node->children) {
            OnNode(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void ImGuiLayer::OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent) {
    // todo: use parent transform to allow World option
    bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
    if (!open) {
        return;
    }
    bool changedOnDrag = false;
    changedOnDrag |= ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1);
    changedOnDrag |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1);
    changedOnDrag |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1);

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        currentGizmoOperation = ImGuizmo::SCALE;
    }
    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
        currentGizmoOperation = ImGuizmo::SCALE;
    }

    if (currentGizmoOperation != ImGuizmo::SCALE) {
        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL)) {
            currentGizmoMode = ImGuizmo::LOCAL;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD)) {
            currentGizmoMode = ImGuizmo::WORLD;
        }
    } else {
        currentGizmoMode = ImGuizmo::LOCAL;
    }
    glm::mat4 modelMat = Node::ComposeTransform(position, rotation, scale, parent);
    glm::mat4 guizmoProj(camera.GetProj());
    guizmoProj[1][1] *= -1;
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::Manipulate(glm::value_ptr(camera.GetView()), glm::value_ptr(guizmoProj), currentGizmoOperation, currentGizmoMode, glm::value_ptr(modelMat));
    modelMat = glm::inverse(parent) * modelMat;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMat), glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));
}

int ImGuiLayer::FindSelected(Ref<Node>& node) {
    auto it = std::find_if(selectedNodes.begin(), selectedNodes.end(), [&](const Ref<Node>& other) {
        return node->uuid == other->uuid;
    });
    return it == selectedNodes.end() ? -1 : it - selectedNodes.begin();
}

void ImGuiLayer::Select(Ref<Node>& node) {
    bool holdingCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    int index = FindSelected(node);
    if (index != -1 && holdingCtrl) {
        selectedNodes.erase(selectedNodes.begin() + index);
    }
    if (index == -1) {
        if (!holdingCtrl) {
            selectedNodes.clear();
        }
        selectedNodes.push_back(node);
    }
}

void ImGuiLayer::ScenePanel(Ref<SceneAsset>& scene) {
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Name: %s", scene->name.c_str());
        for (auto& node : scene->nodes) {
            OnNode(node);
        }
    }
    ImGui::End();
}

bool ImGuiLayer::IsSelected(Ref<Node>& node) {
    return FindSelected(node) != -1;
}

void ImGuiLayer::InspectorPanel(Camera& camera) {
    bool open = ImGui::Begin("Inspector");
    if (open && selectedNodes.size() > 0) {
        // todo: handle multi selection
        Ref<Node>& selected = selectedNodes.back();
        ImGui::InputInt("UUID", (int*)&selected->uuid, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputText("Name", &selected->name);
        OnTransform(camera, selected->position, selected->rotation, selected->scale, selected->parentTransform);
    }
    ImGui::End();
}

void ImGuiLayer::AssetsPanel(AssetManager2& assets) {
    if (ImGui::Begin("Assets")) {
        if (ImGui::CollapsingHeader("Meshes")) {
            for (auto& asset : assets.GetAll<MeshAsset>(ObjectType::MeshAsset)) {
                if (ImGui::TreeNodeEx(&asset->uuid, ImGuiTreeNodeFlags_None, "%s#%d", asset->name.c_str(), asset->uuid)) {
                    ImGui::Text("Indices: %d", asset->indices.size());
                    ImGui::Text("Vertices: %d", asset->vertices.size());
                    ImGui::TreePop();
                }
            }
        }
        if (ImGui::CollapsingHeader("Textures")) {
            for (auto& asset : assets.GetAll<TextureAsset>(ObjectType::TextureAsset)) {
                if (ImGui::TreeNodeEx(&asset->uuid, ImGuiTreeNodeFlags_None, "%s#%d", asset->name.c_str(), asset->uuid)) {
                    ImGui::Text("Width: %d", asset->width);
                    ImGui::Text("Height: %d", asset->height);
                    ImGui::Text("Channels: %d", asset->channels);
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
}