#pragma once

#include "AssetManager2.hpp"

struct ImGuiLayer {
    void SceneOnImGui(Ref<SceneAsset>& scene);
    void AssetManagerOnImGui(AssetManager2& assetManager);
    void NodeOnImGui(Ref<Node>& node);
private:
    Ref<Node> selectedNode;
};