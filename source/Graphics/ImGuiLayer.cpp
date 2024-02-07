#include "ImGuiLayer.h"

#include <imgui/imgui.h>

void ImGuiLayer::NodeOnImGui(Ref<Node>& node) {
    ImGui::PushID(node->uuid);
    ImGuiTreeNodeFlags flags;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.size() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (selectedNode == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    bool open = ImGui::TreeNodeEx(node->name.c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedNode = node;
    }
    if (open) {
        for (auto& child : node->children) {
            NodeOnImGui(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void ImGuiLayer::SceneOnImGui(Ref<SceneAsset>& scene) {
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Name: %s", scene->name.c_str());
        for (auto& node : scene->nodes) {
            NodeOnImGui(node);
        }
    }
    ImGui::End();
}

void ImGuiLayer::AssetManagerOnImGui(AssetManager2& assets) {
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