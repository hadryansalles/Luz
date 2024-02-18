#pragma once

#include "Base.hpp"

struct EditorImpl;
struct Camera;
struct AssetManager;
struct SceneAsset;
struct Node;
struct GPUScene;

namespace vkw {
struct Image;
}
struct ImDrawData;

struct Editor {
    Editor();
    ~Editor();

    void BeginFrame();
    ImDrawData* EndFrame();

    void InspectorPanel(AssetManager& assetManager, Camera& camera, GPUScene& gpuScene);
    void DemoPanel();
    void ScenePanel(Ref<SceneAsset>& scene, Camera& camera);
    void AssetsPanel(AssetManager& assetManager);
    void ProfilerPanel();
    void ProfilerPopup();
    bool ViewportPanel(vkw::Image& image, glm::ivec2& newSize);
    void Select(AssetManager& assetManager, const std::vector<Ref<Node>>& uuids);
private:
    EditorImpl* impl;
};