#include "Editor.h"
#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "Camera.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/ImGuizmo.h>

struct EditorImpl {
    std::vector<Ref<Node>> selectedNodes;
    std::vector<Ref<Node>> copiedNodes;
    bool assetTypeFilter[int(ObjectType::Count)] = {};
    std::string assetNameFilter = "";
    void OnNode(Ref<Node> node);
    void InspectMeshNode(AssetManager& manager, Ref<MeshNode> node);
    void InspectLightNode(AssetManager& manager, Ref<LightNode> node);
    void InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material);
    void OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
    void Select(Ref<Node>& node);
    int FindSelected(Ref<Node>& node);
};

Editor::Editor() {
    impl = new EditorImpl();
    for (int i = 0; i < int(ObjectType::Count); i++) {
        impl->assetTypeFilter[i] = true;
    }
}

Editor::~Editor() {
    delete impl;
}

void EditorImpl::OnNode(Ref<Node> node) {
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

void EditorImpl::OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent) {
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
    ImGui::Separator();
}

int EditorImpl::FindSelected(Ref<Node>& node) {
    auto it = std::find_if(selectedNodes.begin(), selectedNodes.end(), [&](const Ref<Node>& other) {
        return node->uuid == other->uuid;
    });
    return it == selectedNodes.end() ? -1 : it - selectedNodes.begin();
}

void EditorImpl::Select(Ref<Node>& node) {
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

void Editor::ScenePanel(Ref<SceneAsset>& scene) {
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Name: %s", scene->name.c_str());
        ImGui::Text("Add");
        ImGui::SameLine();
        if (ImGui::Button("Empty")) {
            
        }
        ImGui::SameLine();
        if (ImGui::Button("Mesh")) {
            
        }
        ImGui::SameLine();
        if (ImGui::Button("Light")) {
            auto newLight = scene->Add<LightNode>();
            newLight->name = "New Light";
            impl->selectedNodes = { newLight };
        }
        if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& node : scene->nodes) {
                impl->OnNode(node);
            }
            bool pressingCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
            if (pressingCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                impl->copiedNodes = impl->selectedNodes;
            }
            if (pressingCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
                for (auto& node : impl->selectedNodes) {
                    scene->Add(Node::Clone(node));
                }
            }
        }
        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        }
    }
    ImGui::End();
}

void Editor::InspectorPanel(AssetManager& assetManager, Camera& camera) {
    bool open = ImGui::Begin("Inspector");
    if (open && impl->selectedNodes.size() > 0) {
        // todo: handle multi selection
        Ref<Node>& selected = impl->selectedNodes.back();
        // todo: fix uuid on imgui
        ImGui::InputInt("UUID", (int*)&selected->uuid, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputText("Name", &selected->name);
        impl->OnTransform(camera, selected->position, selected->rotation, selected->scale, selected->parentTransform);
        switch (selected->type) {
            case ObjectType::MeshNode: impl->InspectMeshNode(assetManager, std::dynamic_pointer_cast<MeshNode>(selected));
            case ObjectType::LightNode: impl->InspectLightNode(assetManager, std::dynamic_pointer_cast<LightNode>(selected));
        }
    }
    ImGui::End();
}

void EditorImpl::InspectLightNode(AssetManager& manager, Ref<LightNode> node) {
    ImGui::ColorEdit3("Color", glm::value_ptr(node->color));
    ImGui::DragFloat("Intensity", &node->intensity, 0.01, 0, 1000, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat("Radius", &node->radius, 0.1, 0.0001, 10000);
}

void EditorImpl::InspectMeshNode(AssetManager& manager, Ref<MeshNode> node) {
    std::string preview = node->mesh ? node->mesh->name : "UNSELECTED";
    if (ImGui::BeginCombo("Mesh", preview.c_str())) {
        for (const auto& mesh : manager.GetAll<MeshAsset>(ObjectType::MeshAsset)) {
            bool selected = node->mesh ? node->mesh->uuid == mesh->uuid : false;
            if (ImGui::Selectable(mesh->name.c_str(), selected)) {
                node->mesh = mesh;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    preview = node->material ? node->material->name : "UNSELECTED";
    if (ImGui::BeginCombo("Material", preview.c_str())) {
        for (const auto& material : manager.GetAll<MaterialAsset>(ObjectType::MaterialAsset)) {
            bool selected = node->material ? node->material->uuid == material->uuid : false;
            if (ImGui::Selectable(material->name.c_str(), selected)) {
                node->material = material;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        if (node->material) {
            node->material = manager.CloneAsset<MaterialAsset>(node->material);
        } else {
            node->material = manager.CreateAsset<MaterialAsset>("New Material");
        }
    }
    if (node->material) {
        InspectMaterial(manager, node->material);
    }
    ImGui::Separator();
}

void EditorImpl::InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material) {
    ImGui::PushID(material->uuid);
    ImGui::InputText("Name", &material->name);
    ImGui::ColorEdit4("Color", glm::value_ptr(material->color));
    ImGui::ColorEdit3("Emission", glm::value_ptr(material->emission));
    ImGui::DragFloat("Roughness", &material->roughness, 0.005, 0, 1);
    ImGui::DragFloat("Metallic", &material->metallic, 0.005, 0, 1);
    ImGui::PopID();
}

void Editor::AssetsPanel(AssetManager& manager) {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Filter", &impl->assetNameFilter);
    ImGui::Checkbox("Mesh", &impl->assetTypeFilter[int(ObjectType::MeshAsset)]);
    ImGui::SameLine();
    ImGui::Checkbox("Scene", &impl->assetTypeFilter[int(ObjectType::SceneAsset)]);
    ImGui::SameLine();
    ImGui::Checkbox("Texture", &impl->assetTypeFilter[int(ObjectType::TextureAsset)]);
    ImGui::SameLine();
    if (ImGui::Button("All/None")) {
        for (int i = 0; i < int(ObjectType::Count); i++) {
            impl->assetTypeFilter[i] = !impl->assetTypeFilter[int(ObjectType::Count)-1];
        }
    }
    for (auto& asset : manager.GetAll()) {
        if (!impl->assetTypeFilter[int(asset->type)]) {
            continue;
        }
        if (impl->assetNameFilter != "" && asset->name.find(impl->assetNameFilter) == std::string::npos) {
            continue;
        }
        ImGui::PushID(asset->uuid);
        if (ImGui::CollapsingHeader((ObjectTypeName[int(asset->type)] + "-" + asset->name).c_str())) {
            // todo: check type and inspect
        }
        ImGui::PopID();
    }
    ImGui::End();
}