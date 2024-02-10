#include "Luzpch.hpp"

#include "Window.hpp"
#include "Camera.hpp"
#include "FileManager.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanWrapper.h"
#include "AssetManager.hpp"
#include "GPUScene.hpp"
#include "Editor.h"

#include <stb_image.h>

#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_stdlib.h>

#include <GLFW/glfw3.h>

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

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
    bool drawUi = true;
    bool viewportResized = false;
    glm::ivec2 viewportSize = { 64, 48 };
    AssetManager assetManager;
    GPUScene gpuScene;
    Ref<SceneAsset> scene;
    Editor editor;
    Camera mainCamera;

    void WaitToInit(float seconds) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto t1 = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() < seconds * 1000.0f) {
            LUZ_PROFILE_FRAME();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            t1 = std::chrono::high_resolution_clock::now();
        }
    }

    void Setup() {
        LUZ_PROFILE_FUNC();
        SetupImgui();
        //assetManager.LoadProject("assets/default.luz");
        scene = assetManager.GetInitialScene();
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        Window::Create();
        vkw::Init(Window::GetGLFWwindow(), Window::GetWidth(), Window::GetHeight());
        CreateImgui();
        DeferredShading::CreateImages(Window::GetWidth(), Window::GetHeight());
        DeferredShading::CreateShaders();
        gpuScene.Create();
        createUniformProjection();
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        gpuScene.Destroy();
        DeferredShading::Destroy();
        DestroyImgui();
        vkw::Destroy();
        Window::Destroy();
        FinishImgui();
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            Window::Update();
            assetManager.AddAssetsToScene(scene, Window::GetAndClearPaths());
            gpuScene.AddAssets(assetManager);
            // todo: focus camera on selected object
            mainCamera.Update(nullptr);
            drawFrame();
            if (Window::IsKeyPressed(GLFW_KEY_F1)) {
                drawUi = !drawUi;
            }
            if (Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) && Window::IsKeyPressed(GLFW_KEY_S)) {
                LOG_INFO("Request saved");
                assetManager.SaveProject("assets/default.luz");
            }
            if (Window::IsKeyPressed(GLFW_KEY_R)) {
                vkw::WaitIdle();
                DeferredShading::CreateShaders();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            } else if (Window::IsDirty()) {
                Window::ApplyChanges();
            }
        }
        vkw::WaitIdle();
    }

    bool DirtyFrameResources() {
        bool dirty = false;
        dirty |= viewportResized;
        dirty |= vkw::GetSwapChainDirty();
        dirty |= Window::GetFramebufferResized();
        return dirty;
    }

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = mainCamera.GetProj() * mainCamera.GetView() * glm::vec4(position, 1.0f);
        ImVec2 screenSpace = ImVec2(cameraSpace.x / cameraSpace.w, cameraSpace.y / cameraSpace.w);
        glm::ivec2 ext = viewportSize;
        screenSpace.x = (screenSpace.x + 1.0) * ext.x/2.0;
        screenSpace.y = (screenSpace.y + 1.0) * ext.y/2.0;
        return screenSpace;
    }

    void imguiDrawFrame() {
        LUZ_PROFILE_FUNC();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        //ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        ImGui::ShowDemoWindow();

        if (ImGui::Begin("Viewport", 0)) {
            ImGui::BeginChild("##ChildViewport");
            glm::ivec2 newViewportSize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
            if (newViewportSize != viewportSize) {
                viewportResized = true;
                viewportSize = newViewportSize;
            }
            DeferredShading::ViewportOnImGui();
            ImGui::EndChild();
        }
        ImGui::End();

        if (ImGui::Begin("Luz Engine")) {
            if (ImGui::BeginTabBar("LuzEngineMainTab")) {
                if (ImGui::BeginTabItem("Configuration")) {
                    Window::OnImgui();
                    mainCamera.OnImgui();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Profiler")) {
            std::map<std::string, float> timeTable;
            vkw::GetTimeStamps(timeTable);
            for (const auto& pair : timeTable) {
                ImGui::Text("%s: %.3f", pair.first.c_str(), pair.second);
            }
        }
        ImGui::End();

        DeferredShading::OnImgui(0);

        editor.AssetsPanel(assetManager);
        editor.ScenePanel(scene);
        editor.InspectorPanel(assetManager, mainCamera);

        ImGui::Render();
        imguiDrawData = ImGui::GetDrawData();
    }

    void updateCommandBuffer() {
        gpuScene.UpdateResources(scene, mainCamera);
        LUZ_PROFILE_FUNC();
        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        gpuScene.UpdateResourcesGPU();

        vkw::CmdBarrier();

        auto totalTS = vkw::CmdBeginTimeStamp("GPU::Total");

        DeferredShading::OpaqueConstants constants;
        constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
        constants.modelBufferIndex = gpuScene.GetModelsBuffer();

        auto opaqueTS = vkw::CmdBeginTimeStamp("GPU::OpaquePass");
        DeferredShading::BeginOpaquePass();

        for (GPUModel& model : gpuScene.GetMeshModels()) {
            constants.modelID = model.modelRID;
            vkw::CmdPushConstants(&constants, sizeof(constants));
            vkw::CmdDrawMesh(model.mesh.vertexBuffer, model.mesh.indexBuffer, model.mesh.indexCount);
        }

        // todo: light gizmos

        DeferredShading::EndPass();
        vkw::CmdEndTimeStamp(opaqueTS);

        auto lightTS = vkw::CmdBeginTimeStamp("LightPass");
        DeferredShading::LightConstants lightPassConstants;
        lightPassConstants.sceneBufferIndex = constants.sceneBufferIndex;
        lightPassConstants.frameID = frameCount;
        DeferredShading::LightPass(lightPassConstants);
        vkw::CmdEndTimeStamp(lightTS);

        auto composeTS = vkw::CmdBeginTimeStamp("GPU::ComposePass");
        DeferredShading::ComposePass();
        vkw::CmdEndTimeStamp(composeTS);

        vkw::CmdBeginPresent();
        auto imguiTS = vkw::CmdBeginTimeStamp("GPU::ImGui");
        if (drawUi) {
            vkw::CmdDrawImGui(imguiDrawData);
        }
        vkw::CmdEndTimeStamp(imguiTS);
        vkw::CmdEndPresent();
        vkw::CmdEndTimeStamp(totalTS);
    }

    void drawFrame() {
        LUZ_PROFILE_FUNC();
        imguiDrawFrame();
        vkw::AcquireImage();
        if (vkw::GetSwapChainDirty()) {
            return;
        }
        updateCommandBuffer();
        vkw::SubmitAndPresent();

        frameCount = (frameCount + 1) % (1 << 15);
    }

    void RecreateFrameResources() {
        LUZ_PROFILE_FUNC();
        // busy wait while the window is minimized
        while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
            Window::WaitEvents();
        }
        if (viewportSize.x == 0 || viewportSize.y == 0) {
            return;
        }
        vkw::WaitIdle();
        if (Window::GetFramebufferResized()) {
            Window::UpdateFramebufferSize();
            vkw::OnSurfaceUpdate(Window::GetWidth(), Window::GetHeight());
        }
        DeferredShading::CreateImages(viewportSize.x, viewportSize.y);
        createUniformProjection();
        viewportResized = false;
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        mainCamera.SetExtent(viewportSize.x, viewportSize.y);
    }

    void SetupImgui() {
        IMGUI_CHECKVERSION();
    }

    void CreateImgui() {
        LUZ_PROFILE_FUNC();
        ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWwindow(), true);
        vkw::InitImGui();
    }

    void DestroyImgui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }
    
    void FinishImgui() {
        ImGui::DestroyContext();
    }
};

int main() {
    Log::Init();
    LuzApplication app;
    app.run();

    return EXIT_SUCCESS;
}