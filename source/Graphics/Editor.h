#pragma once

#include "Base.hpp"

struct EditorImpl;
struct Camera;
struct AssetManager;
struct SceneAsset;

namespace vkw {
struct Image;
}
struct ImDrawData;

struct Editor {
    Editor();
    ~Editor();

    void BeginFrame();
    ImDrawData* EndFrame();

    void InspectorPanel(AssetManager& assetManager, Camera& camera);
    void DemoPanel();
    void ScenePanel(Ref<SceneAsset>& scene, Camera& camera);
    void AssetsPanel(AssetManager& assetManager);
    void ProfilerPanel();
    void ProfilerPopup();
    bool ViewportPanel(vkw::Image& image, glm::ivec2& newSize);
private:
    EditorImpl* impl;
};