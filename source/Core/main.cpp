#include "Luzpch.hpp"

#include "Window.hpp"
#include "CameraController.hpp"
#include "FileManager.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanWrapper.h"
#include "AssetManager.hpp"
#include "GPUScene.hpp"
#include "Editor.h"
#include "DebugDraw.h"
#include "LuzCommon.h"

#include <stb_image.h>

#include <imgui/imgui_stdlib.h>
#include <imgui/ImGuizmo.h>

#include <GLFW/glfw3.h>

class LuzApplication {
public:
    void run() {
        Setup();
        Create();
        MainLoop();
        Finish();
    }

private:
    u32 frameCount = 0;
    ImDrawData* imguiDrawData = nullptr;
    glm::ivec2 viewportSize = { 64, 48 };
    glm::ivec2 newViewportSize = viewportSize;
    AssetManager assetManager;
    GPUScene gpuScene;
    Ref<SceneAsset> scene;
    Ref<CameraNode> camera;
    Editor editor;
    CameraController cameraController;
    bool viewportHovered = false;
    bool fullscreen = false;
    bool batterySaver = LUZ_BATTERY_SAVER;
    DeferredRenderer::Output outputMode = DeferredRenderer::Output::Light;

    std::chrono::high_resolution_clock::time_point lastFrameTime = {};
    std::chrono::high_resolution_clock::time_point lastCameraTime = {};

    struct CacheData {
        char projectPath[1024] = "assets/default.luz";
        char binPath[1024] = "assets/default.luzbin";
    };
    CacheData cacheData;
    void ReadCache() {
        std::ifstream cacheFile("data.luzcache", std::ios::binary);
        if (cacheFile.is_open()) {
            cacheFile.read((char*)&cacheData, sizeof(cacheData));
            cacheFile.close();
        }
        if (!std::filesystem::exists(cacheData.projectPath) || !std::filesystem::exists(cacheData.binPath)) {
            Log::Error("Cache file is corrupted. Resetting to default.");
            cacheData = {};
            WriteCache();
        }
    }
    void WriteCache() {
        std::ofstream cacheFile("data.luzcache", std::ios::binary);
        if (cacheFile.is_open()) {
            cacheFile.write((char*)&cacheData, sizeof(cacheData));
            cacheFile.close();
        }
    }

    void Setup() {
        LUZ_PROFILE_FUNC();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ReadCache();
        assetManager.LoadProject(cacheData.projectPath, cacheData.binPath);
        scene = assetManager.GetInitialScene();
        camera = assetManager.GetMainCamera(scene);
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        Window::Create();
        Window::SetTitle("Luz Engine - " + assetManager.GetProjectName());
        vkw::Init(Window::GetGLFWwindow(), Window::GetWidth(), Window::GetHeight());
        DeferredRenderer::CreateImages(Window::GetWidth(), Window::GetHeight());
        DeferredRenderer::CreateShaders();
        gpuScene.Create();
        DebugDraw::Create();
        camera->extent = { viewportSize.x, viewportSize.y };
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        DebugDraw::Destroy();
        gpuScene.Destroy();
        DeferredRenderer::Destroy();
        vkw::Destroy();
        Window::Destroy();
        FinishImgui();
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            LUZ_PROFILE_NAMED("MainLoop");
            if (assetManager.HasLoadRequest()) {
                vkw::WaitIdle();
                assetManager.LoadRequestedProject();
                scene = assetManager.GetInitialScene();
                camera = assetManager.GetMainCamera(scene);
                camera->extent = { viewportSize.x, viewportSize.y };
                cameraController.Reset();
                strcpy(cacheData.projectPath, assetManager.GetCurrentProjectPath().string().c_str());
                strcpy(cacheData.binPath, assetManager.GetCurrentBinPath().string().c_str());
                Window::SetTitle("Luz Engine - " + assetManager.GetProjectName());
                WriteCache();
            }
            if (const auto paths = Window::GetAndClearPaths(); paths.size()) {
                auto newNodes = assetManager.AddAssetsToScene(scene, paths);
                if (newNodes.size()) {
                    editor.Select(assetManager, newNodes);
                }
            }
            gpuScene.AddAssets(assetManager);
            // todo: focus camera on selected object
            {
                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastCameraTime).count() / 1000000.0f;
                cameraController.Update(scene, camera, viewportHovered, deltaTime);
                lastCameraTime = currentTime;
            }
            DrawFrame();
            bool ctrlPressed = Window::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL);
            if (ctrlPressed && Window::IsKeyPressed(GLFW_KEY_S)) {
                assetManager.SaveProject("assets/default.luz", "assets/default.luzbin");
            }
            if (Window::IsKeyPressed(GLFW_KEY_F5)) {
                vkw::WaitIdle();
                DeferredRenderer::CreateShaders();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            }
            BatterySaver();
            Window::Update();
        }
        vkw::WaitIdle();
    }

    bool DirtyFrameResources() {
        bool dirty = false;
        dirty |= (newViewportSize != viewportSize);
        dirty |= vkw::GetSwapChainDirty();
        dirty |= Window::GetFramebufferResized();
        dirty |= Window::IsDirty();
        return dirty;
    }

    void BatterySaver() {
        LUZ_PROFILE_NAMED("BatterySaver");
        if (!batterySaver) {
            return;
        }
        float targetFrameTime = 33.333f;
        float elapsedTime = 0.0f;
        do {
            auto currentTime = std::chrono::high_resolution_clock::now();
            elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastFrameTime).count() / 1000.0f;
        } while (elapsedTime < targetFrameTime);

        lastFrameTime = std::chrono::high_resolution_clock::now();
    }

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = camera->GetProj() * camera->GetView() * glm::vec4(position, 1.0f);
        ImVec2 screenSpace = ImVec2(cameraSpace.x / cameraSpace.w, cameraSpace.y / cameraSpace.w);
        glm::ivec2 ext = viewportSize;
        screenSpace.x = (screenSpace.x + 1.0f) * ext.x/2.0f;
        screenSpace.y = (screenSpace.y + 1.0f) * ext.y/2.0f;
        return screenSpace;
    }

    void DrawEditor() {
        LUZ_PROFILE_FUNC();
        LUZ_PROFILE_NAMED("DrawEditor");

        if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
            fullscreen = !fullscreen;
            if (fullscreen) {
                Window::SetMode(WindowMode::FullScreen);
                glfwSetInputMode(Window::GetGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                Window::SetMode(WindowMode::Windowed);
                glfwSetInputMode(Window::GetGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F7)) {
            outputMode = DeferredRenderer::Output((outputMode + 1) % DeferredRenderer::Output::Count);
        }

        editor.BeginFrame();

        if (!fullscreen) {
            viewportHovered = editor.ViewportPanel(DeferredRenderer::GetComposedImage(), newViewportSize);
            editor.ProfilerPanel();
            editor.AssetsPanel(assetManager);
            editor.DemoPanel();
            editor.ScenePanel(scene);
            editor.InspectorPanel(assetManager, camera, gpuScene);
            editor.DebugDrawPanel();
        } else {
            newViewportSize = { Window::GetWidth(), Window::GetHeight() };
            viewportHovered = true;
        }
        editor.ProfilerPopup();

        imguiDrawData = editor.EndFrame();
    }

    void RenderFrame() {
        gpuScene.UpdateResources(scene, camera);
        DebugDraw::Update();

        LUZ_PROFILE_FUNC();

        vkw::BeginCommandBuffer(vkw::Queue::Graphics);

        gpuScene.UpdateResourcesGPU();
        gpuScene.UpdateLineResources();

        vkw::CmdBarrier();

        auto totalTS = vkw::CmdBeginTimeStamp("Total");

        OpaqueConstants constants;
        constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
        constants.modelBufferIndex = gpuScene.GetModelsBuffer();

        auto opaqueTS = vkw::CmdBeginTimeStamp("OpaquePass");
        DeferredRenderer::BeginOpaquePass();

        {
            LUZ_PROFILE_NAMED("RenderModels");
            auto& allModels = gpuScene.GetMeshModels();
            for (GPUModel& model : allModels) {
                constants.modelID = model.modelRID;
                vkw::CmdPushConstants(&constants, sizeof(constants));
                vkw::CmdDrawMesh(model.mesh.vertexBuffer, model.mesh.indexBuffer, model.mesh.indexCount);
            }
        }

        // todo: light gizmos

        DeferredRenderer::EndPass();
        vkw::CmdEndTimeStamp(opaqueTS);

        auto shadowMapTS = vkw::CmdBeginTimeStamp("ShadowMaps");
        for (auto& light : scene->GetAll<LightNode>(ObjectType::LightNode)) {
            DeferredRenderer::ShadowMapPass(light, scene, gpuScene);
        }
        vkw::CmdEndTimeStamp(shadowMapTS);

        auto lightTS = vkw::CmdBeginTimeStamp("LightPass");
        DeferredRenderer::LightConstants lightPassConstants;
        lightPassConstants.sceneBufferIndex = constants.sceneBufferIndex;
        lightPassConstants.modelBufferIndex = constants.modelBufferIndex;
        lightPassConstants.frameID = frameCount;
        DeferredRenderer::LightPass(lightPassConstants);
        vkw::CmdEndTimeStamp(lightTS);

        auto volumetricTS = vkw::CmdBeginTimeStamp("VolumetricLightPass");
        if (gpuScene.AnyVolumetricLight()) {
            DeferredRenderer::ScreenSpaceVolumetricLightPass(gpuScene, frameCount);
            DeferredRenderer::ShadowMapVolumetricLightPass(gpuScene, frameCount);
        }
        vkw::CmdEndTimeStamp(volumetricTS);

        auto taaTS = vkw::CmdBeginTimeStamp("TAAPass");
        DeferredRenderer::TAAPass(gpuScene, scene);
        vkw::CmdEndTimeStamp(taaTS);

        auto lineTS = vkw::CmdBeginTimeStamp("LineRenderingPass");
        DeferredRenderer::LineRenderingPass(gpuScene);
        vkw::CmdEndTimeStamp(lineTS);

        auto histogramTS = vkw::CmdBeginTimeStamp("LuminanceHistogramPass");
        DeferredRenderer::LuminanceHistogramPass();
        vkw::CmdEndTimeStamp(histogramTS);

        auto composeTS = vkw::CmdBeginTimeStamp("ComposePass");
        if (fullscreen) {
            vkw::CmdBeginPresent();
            DeferredRenderer::ComposePass(false, outputMode, scene);
        }  else {
            DeferredRenderer::ComposePass(true, outputMode, scene);
            vkw::CmdBeginPresent();
        }
        vkw::CmdEndTimeStamp(composeTS);

        auto imguiTS = vkw::CmdBeginTimeStamp("ImGui");
        vkw::CmdDrawImGui(imguiDrawData);
        vkw::CmdEndTimeStamp(imguiTS);

        vkw::CmdEndPresent();
        vkw::CmdEndTimeStamp(totalTS);

        DeferredRenderer::SwapLightHistory();
    }

    void DrawFrame() {
        LUZ_PROFILE_FUNC();
        DrawEditor();
        RenderFrame();
        if (vkw::GetSwapChainDirty()) {
            return;
        }
        vkw::SubmitAndPresent();
        frameCount = (frameCount + 1) % (1 << 15);
    }

    void RecreateFrameResources() {
        LUZ_PROFILE_FUNC();
        // busy wait while the window is minimized
        while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
            Window::WaitEvents();
        }
        viewportSize = newViewportSize;
        if (viewportSize.x == 0 || viewportSize.y == 0) {
            return;
        }
        vkw::WaitIdle();
        if (Window::GetFramebufferResized() || Window::IsDirty()) {
            if (Window::IsDirty()) {
                Window::ApplyChanges();
            }
            Window::UpdateFramebufferSize();
            vkw::OnSurfaceUpdate(Window::GetWidth(), Window::GetHeight());
        }
        DeferredRenderer::CreateImages(viewportSize.x, viewportSize.y);
        camera->extent = {viewportSize.x, viewportSize.y};
    }

    void FinishImgui() {
        ImGui::DestroyContext();
    }

};

bool CheckAssetsDirectory() {
    return std::filesystem::is_directory("assets/");
}

int main() {
    Logger::Init();
    if (!CheckAssetsDirectory()) {
        Log::Error("Wrong working directory. Run from main directory (\"Luz/\")");
        Log::Error("(Linux): ./Luz/bin/Luz");
        Log::Error("(Windows): ./Luz/bin/Luz.exe");
        return 0;
    }
    LuzApplication app;
    app.run();
    return 0;
}