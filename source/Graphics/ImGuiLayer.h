#pragma once

#include "AssetManager.hpp"
#include "Camera.hpp"

struct ImGuiLayer {
    void InspectorPanel(AssetManager& assetManager, Camera& camera);
    void ScenePanel(Ref<SceneAsset>& scene);
    void AssetsPanel(AssetManager& assetManager);
    bool IsSelected(Ref<Node>& node);
private:
    std::vector<Ref<Node>> selectedNodes;
    std::vector<Ref<Node>> copiedNodes;
    void OnNode(Ref<Node> node);
    void InspectMeshNode(AssetManager& manager, Ref<MeshNode> node);
    void InspectLightNode(AssetManager& manager, Ref<LightNode> node);
    void InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material);
    void OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
    void Select(Ref<Node>& node);
    int FindSelected(Ref<Node>& node);
};