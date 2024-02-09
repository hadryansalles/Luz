#pragma once

#include "Base.hpp"

struct EditorImpl;
struct Camera;
struct AssetManager;
struct SceneAsset;

struct Editor {
    Editor();
    ~Editor();
    void InspectorPanel(AssetManager& assetManager, Camera& camera);
    void ScenePanel(Ref<SceneAsset>& scene);
    void AssetsPanel(AssetManager& assetManager);
private:
    EditorImpl* impl;
};