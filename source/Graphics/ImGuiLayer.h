#pragma once

#include "AssetManager2.hpp"
#include "Camera.hpp"

struct ImGuiLayer {
    void InspectorPanel(Camera& camera);
    void ScenePanel(Ref<SceneAsset>& scene);
    void AssetsPanel(AssetManager2& assetManager);
    bool IsSelected(Ref<Node>& node);
private:
    std::vector<Ref<Node>> selectedNodes;
    void OnNode(Ref<Node>& node);
    void OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
    void Select(Ref<Node>& node);
    int FindSelected(Ref<Node>& node);
};