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
    Editor editor;
    Camera mainCamera;
    bool viewportHovered = false;
    bool fullscreen = false;

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
        IMGUI_CHECKVERSION();
        //assetManager.LoadProject("assets/default.luz");
        scene = assetManager.GetInitialScene();
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        Window::Create();
        vkw::Init(Window::GetGLFWwindow(), Window::GetWidth(), Window::GetHeight());
        DeferredShading::CreateImages(Window::GetWidth(), Window::GetHeight());
        DeferredShading::CreateShaders();
        gpuScene.Create();
        mainCamera.SetExtent(viewportSize.x, viewportSize.y);
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        gpuScene.Destroy();
        DeferredShading::Destroy();
        vkw::Destroy();
        Window::Destroy();
        FinishImgui();
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            LUZ_PROFILE_NAMED("Total");
            Window::Update();
            assetManager.AddAssetsToScene(scene, Window::GetAndClearPaths());
            gpuScene.AddAssets(assetManager);
            // todo: focus camera on selected object
            mainCamera.Update(nullptr, viewportHovered);
            DrawFrame();
            bool ctrlPressed = Window::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL);
            if (ctrlPressed && Window::IsKeyPressed(GLFW_KEY_S)) {
                assetManager.SaveProject("assets/default.luz");
            }
            if (Window::IsKeyPressed(GLFW_KEY_F5)) {
                vkw::WaitIdle();
                DeferredShading::CreateShaders();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            }
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

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = mainCamera.GetProj() * mainCamera.GetView() * glm::vec4(position, 1.0f);
        ImVec2 screenSpace = ImVec2(cameraSpace.x / cameraSpace.w, cameraSpace.y / cameraSpace.w);
        glm::ivec2 ext = viewportSize;
        screenSpace.x = (screenSpace.x + 1.0) * ext.x/2.0;
        screenSpace.y = (screenSpace.y + 1.0) * ext.y/2.0;
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

        editor.BeginFrame();

        if (!fullscreen) {
            viewportHovered = editor.ViewportPanel(DeferredShading::GetComposedImage(), newViewportSize);
            editor.ProfilerPanel();
            editor.AssetsPanel(assetManager);
            editor.DemoPanel();
            editor.ScenePanel(scene, mainCamera);
            editor.InspectorPanel(assetManager, mainCamera);
        } else {
            newViewportSize = { Window::GetWidth(), Window::GetHeight() };
            viewportHovered = true;
        }
        editor.ProfilerPopup();

        imguiDrawData = editor.EndFrame();
    }

    void RenderFrame() {
        gpuScene.UpdateResources(scene, mainCamera);
        LUZ_PROFILE_FUNC();
        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        gpuScene.UpdateResourcesGPU();

        vkw::CmdBarrier();

        auto totalTS = vkw::CmdBeginTimeStamp("Total");

        DeferredShading::OpaqueConstants constants;
        constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
        constants.modelBufferIndex = gpuScene.GetModelsBuffer();

        auto opaqueTS = vkw::CmdBeginTimeStamp("OpaquePass");
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

        auto composeTS = vkw::CmdBeginTimeStamp("ComposePass");
        if (fullscreen) {
            vkw::CmdBeginPresent();
            DeferredShading::ComposePass(false);
        }  else {
            DeferredShading::ComposePass(true);
            vkw::CmdBeginPresent();
        }
        vkw::CmdEndTimeStamp(composeTS);

        auto imguiTS = vkw::CmdBeginTimeStamp("ImGui");
        vkw::CmdDrawImGui(imguiDrawData);
        vkw::CmdEndTimeStamp(imguiTS);

        vkw::CmdEndPresent();
        vkw::CmdEndTimeStamp(totalTS);

    }

    void DrawFrame() {
        LUZ_PROFILE_FUNC();
        DrawEditor();
        vkw::AcquireImage();
        if (vkw::GetSwapChainDirty()) {
            return;
        }
        RenderFrame();
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
        DeferredShading::CreateImages(viewportSize.x, viewportSize.y);
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        mainCamera.SetExtent(viewportSize.x, viewportSize.y);
    }

    void FinishImgui() {
        ImGui::DestroyContext();
    }
};

int main() {
    Logger::Init();
    LuzApplication app;
    app.run();
    return 0;
}