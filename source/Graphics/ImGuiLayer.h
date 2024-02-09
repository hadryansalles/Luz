#pragma once

#include "AssetManager2.hpp"
#include "Camera.hpp"

struct ImGuiLayer {
    void InspectorPanel(AssetManager2& assetManager, Camera& camera);
    void ScenePanel(Ref<SceneAsset>& scene);
    void AssetsPanel(AssetManager2& assetManager);
    bool IsSelected(Ref<Node>& node);
private:
    std::vector<Ref<Node>> selectedNodes;
    std::vector<Ref<Node>> copiedNodes;
    void OnNode(Ref<Node> node);
    void InspectMesh(AssetManager2& manager, Ref<MeshNode> node);
    void InspectMaterial(AssetManager2& manager, Ref<MaterialAsset> material);
    void OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
    void Select(Ref<Node>& node);
    int FindSelected(Ref<Node>& node);
};