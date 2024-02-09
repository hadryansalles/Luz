#include "Luzpch.hpp"

#include "Window.hpp"
#include "Camera.hpp"
#include "FileManager.hpp"
#include "AssetManager.hpp"
#include "Scene.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanWrapper.h"
#include "AssetManager2.hpp"
#include "GPUScene.hpp"
#include "ImGuiLayer.h"

#include <stb_image.h>

#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_stdlib.h>

// #define GLFW_INCLUDE_VULKAN
// glfw will include vulkan and its own definitions
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
    AssetManager2 assetManager;
    GPUScene gpuScene;
    Ref<SceneAsset> scene;
    ImGuiLayer imguiLayer;

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
        AssetManager::Setup();
        SetupImgui();
        Scene::Setup();
        // todo: asset manager load default file
        assetManager.LoadProject("assets/default.luz");
        scene = assetManager.GetInitialScene();
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        Window::Create();
        vkw::Init(Window::GetGLFWwindow(), Window::GetWidth(), Window::GetHeight());
        CreateImgui();
        DeferredShading::CreateImages(Window::GetWidth(), Window::GetHeight());
        DeferredShading::CreateShaders();
        AssetManager::Create();
        gpuScene.Create();
        Scene::CreateResources();
        createUniformProjection();
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        gpuScene.Destroy();
        Scene::DestroyResources();
        DeferredShading::Destroy();
        AssetManager::Destroy();
        DestroyImgui();
        vkw::Destroy();
        Window::Destroy();
        AssetManager::Finish();
        // todo: asset manager save scene
        FinishImgui();
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            Window::Update();
            assetManager.AddAssetsToScene(scene, Window::GetAndClearPaths());
            gpuScene.AddAssets(assetManager);
            gpuScene.UpdateResources(scene, Scene::camera);
            AssetManager::UpdateResources();
            Transform* selectedTransform = nullptr;
            if (Scene::selectedEntity != nullptr) {
                selectedTransform = &Scene::selectedEntity->transform;
            }
            Scene::camera.Update(selectedTransform);
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
        glm::vec4 cameraSpace = Scene::camera.GetProj() * Scene::camera.GetView() * glm::vec4(position, 1.0f);
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
                    Scene::camera.OnImgui();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Assets-old")) {
                    AssetManager::OnImgui();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Inspector_old")) {
            if (Scene::selectedEntity != nullptr) {
                Scene::InspectEntity(Scene::selectedEntity);
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

        bool sceneOpen = true;
        if (ImGui::Begin("Scene_old")) {
            Scene::OnImgui();
        }
        ImGui::End();

        imguiLayer.AssetsPanel(assetManager);
        imguiLayer.ScenePanel(scene);
        imguiLayer.InspectorPanel(assetManager, Scene::camera);

        ImGui::Render();
        imguiDrawData = ImGui::GetDrawData();
    }

    void updateCommandBuffer() {
        LUZ_PROFILE_FUNC();
        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        auto totalTS = vkw::CmdBeginTimeStamp("GPU::Total");

        vkw::CmdCopy(Scene::sceneBuffer, &Scene::scene, sizeof(Scene::scene));
        vkw::CmdCopy(Scene::modelsBuffer, &Scene::models, sizeof(Scene::models));
        vkw::CmdTimeStamp("GPU::BuildTLAS", [&] {
            std::vector<vkw::BLASInstance> vkwInstances(Scene::modelEntities.size());
            for (int i = 0; i < Scene::modelEntities.size(); i++) {
                MeshResource& mesh = AssetManager::meshes[Scene::modelEntities[i]->mesh];
                vkwInstances[i] = {
                    .blas = mesh.blas,
                    .modelMat = Scene::modelEntities[i]->transform.GetMatrix(),
                    .customIndex = Scene::modelEntities[i]->id,
                };
            }
            vkw::CmdBuildTLAS(Scene::tlas, vkwInstances);
        });
        vkw::CmdBarrier();

        gpuScene.UpdateResourcesGPU();

        DeferredShading::OpaqueConstants constants;
        //constants.sceneBufferIndex = Scene::sceneBuffer.RID();
        //constants.modelBufferIndex = Scene::modelsBuffer.RID();
        constants.sceneBufferIndex = gpuScene.GetSceneBuffer();
        constants.modelBufferIndex = gpuScene.GetModelsBuffer();

        if (gpuScene.GetMeshModels().size() > 0) {
            auto opaqueTS = vkw::CmdBeginTimeStamp("GPU::OpaquePass");
            DeferredShading::BeginOpaquePass();

            //for (Model* model : Scene::modelEntities) {
            //    constants.modelID = model->id;
            //    vkw::CmdPushConstants(&constants, sizeof(constants));
            //    DeferredShading::RenderMesh(model->mesh);
            //}
            for (GPUModel& model : gpuScene.GetMeshModels()) {
                constants.modelID = model.modelRID;
                vkw::CmdPushConstants(&constants, sizeof(constants));
                vkw::CmdDrawMesh(model.mesh.vertexBuffer, model.mesh.indexBuffer, model.mesh.indexCount);
            }

            //if (Scene::renderLightGizmos) {
            //    for (Light* light : Scene::lightEntities) {
            //        constants.modelID = light->id;
            //        vkw::CmdPushConstants(&constants, sizeof(constants));
            //        DeferredShading::RenderMesh(Scene::lightMeshes[light->block.type]);
            //    }
            //}

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
        } else {
            DeferredShading::PrepareEmptyPresent();
        }

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
        updateUniformBuffer();
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
        Scene::camera.SetExtent(viewportSize.x, viewportSize.y);
    }

    void updateUniformBuffer() {
        LUZ_PROFILE_FUNC();
        Scene::UpdateResources();
    }

    void SetupImgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //ImGui::Dock
        {
            constexpr auto ColorFromBytes = [](uint8_t r, uint8_t g, uint8_t b)
            {
                return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
            };

            auto& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            const ImVec4 bgColor           = ColorFromBytes(37, 37, 38);
            const ImVec4 lightBgColor      = ColorFromBytes(82, 82, 85);
            const ImVec4 veryLightBgColor  = ColorFromBytes(90, 90, 95);

            const ImVec4 panelColor        = ColorFromBytes(51, 51, 55);
            const ImVec4 panelHoverColor   = ColorFromBytes(29, 151, 236);
            const ImVec4 panelActiveColor  = ColorFromBytes(0, 119, 200);

            const ImVec4 textColor         = ColorFromBytes(255, 255, 255);
            const ImVec4 textDisabledColor = ColorFromBytes(151, 151, 151);
            const ImVec4 borderColor       = ColorFromBytes(78, 78, 78);

            colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.25f);
            colors[ImGuiCol_Text]                 = textColor;
            colors[ImGuiCol_TextDisabled]         = textDisabledColor;
            colors[ImGuiCol_TextSelectedBg]       = panelActiveColor;
            colors[ImGuiCol_ChildBg]              = bgColor;
            colors[ImGuiCol_PopupBg]              = bgColor;
            colors[ImGuiCol_Border]               = borderColor;
            colors[ImGuiCol_BorderShadow]         = borderColor;
            colors[ImGuiCol_FrameBg]              = panelColor;
            colors[ImGuiCol_FrameBgHovered]       = panelHoverColor;
            colors[ImGuiCol_FrameBgActive]        = panelActiveColor;
            colors[ImGuiCol_TitleBg]              = bgColor;
            colors[ImGuiCol_TitleBgActive]        = bgColor;
            colors[ImGuiCol_TitleBgCollapsed]     = bgColor;
            colors[ImGuiCol_MenuBarBg]            = panelColor;
            colors[ImGuiCol_ScrollbarBg]          = panelColor;
            colors[ImGuiCol_ScrollbarGrab]        = lightBgColor;
            colors[ImGuiCol_ScrollbarGrabHovered] = veryLightBgColor;
            colors[ImGuiCol_ScrollbarGrabActive]  = veryLightBgColor;
            colors[ImGuiCol_CheckMark]            = panelActiveColor;
            colors[ImGuiCol_SliderGrab]           = panelHoverColor;
            colors[ImGuiCol_SliderGrabActive]     = panelActiveColor;
            colors[ImGuiCol_Button]               = panelColor;
            colors[ImGuiCol_ButtonHovered]        = panelHoverColor;
            colors[ImGuiCol_ButtonActive]         = panelHoverColor;
            colors[ImGuiCol_Header]               = panelColor;
            colors[ImGuiCol_HeaderHovered]        = panelHoverColor;
            colors[ImGuiCol_HeaderActive]         = panelActiveColor;
            colors[ImGuiCol_Separator]            = borderColor;
            colors[ImGuiCol_SeparatorHovered]     = borderColor;
            colors[ImGuiCol_SeparatorActive]      = borderColor;
            colors[ImGuiCol_ResizeGrip]           = bgColor;
            colors[ImGuiCol_ResizeGripHovered]    = panelColor;
            colors[ImGuiCol_ResizeGripActive]     = lightBgColor;
            colors[ImGuiCol_PlotLines]            = panelActiveColor;
            colors[ImGuiCol_PlotLinesHovered]     = panelHoverColor;
            colors[ImGuiCol_PlotHistogram]        = panelActiveColor;
            colors[ImGuiCol_PlotHistogramHovered] = panelHoverColor;
            colors[ImGuiCol_DragDropTarget]       = bgColor;
            colors[ImGuiCol_NavHighlight]         = bgColor;
            colors[ImGuiCol_DockingPreview]       = panelActiveColor;
            colors[ImGuiCol_Tab]                  = bgColor;
            colors[ImGuiCol_TabActive]            = panelActiveColor;
            colors[ImGuiCol_TabUnfocused]         = bgColor;
            colors[ImGuiCol_TabUnfocusedActive]   = panelActiveColor;
            colors[ImGuiCol_TabHovered]           = panelHoverColor;

            style.WindowRounding    = 0.0f;
            style.ChildRounding     = 0.0f;
            style.FrameRounding     = 0.0f;
            style.GrabRounding      = 0.0f;
            style.PopupRounding     = 0.0f;
            style.ScrollbarRounding = 0.0f;
            style.TabRounding       = 0.0f;
        }
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