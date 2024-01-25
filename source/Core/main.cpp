#include "Luzpch.hpp" 

#include "ImageManager.hpp"
#include "Window.hpp"
#include "Camera.hpp"
#include "Shader.hpp"
#include "GraphicsPipelineManager.hpp"
#include "FileManager.hpp"
#include "AssetManager.hpp"
#include "Scene.hpp"
#include "RayTracing.hpp"
#include "DeferredRenderer.hpp"
#include "VulkanLayer.h"

#include <stb_image.h>

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_stdlib.h>
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

void CheckVulkanResult(VkResult res) {
    if (res == 0) {
        return;
    }
    std::cerr << "vulkan error during some imgui operation: " << res << '\n';
    if (res < 0) {
        throw std::runtime_error("");
    }
}

class LuzApplication {
public:
    void run() {
        // WaitToInit(4);
        Setup();
        Create();
        MainLoop();
        Finish();
    }

private:
    u32 frameCount = 0;
    ImDrawData* imguiDrawData = nullptr;
    bool drawUi = true;

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
        DeferredShading::Setup();
        AssetManager::Setup();
        SetupImgui();
        Scene::Setup();
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        CreateVulkan();
    }

    void CreateVulkan() {
        LUZ_PROFILE_FUNC();
        Window::Create();
        vkw::Init(Window::GetGLFWwindow(), Window::GetWidth(), Window::GetHeight());
        DEBUG_TRACE("Finish creating SwapChain.");
        GraphicsPipelineManager::Create();
        CreateImgui();
        RayTracing::Create();
        DeferredShading::Create();
        AssetManager::Create();
        Scene::CreateResources();
        createUniformProjection();
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        DestroyVulkan();
        AssetManager::Finish();
        FinishImgui();
    }

    void DestroyVulkan() {
        LUZ_PROFILE_FUNC();
        DestroyFrameResources();
        DestroyImgui();
        RayTracing::Destroy();
        GraphicsPipelineManager::Destroy();
        AssetManager::Destroy();
        vkw::Destroy();
        Window::Destroy();
    }

    void DestroyFrameResources() {
        LUZ_PROFILE_FUNC();
        Scene::DestroyResources();
        DeferredShading::Destroy();
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            Window::Update();
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
            if (Window::IsKeyPressed(GLFW_KEY_R)) {
                vkDeviceWaitIdle(vkw::ctx().device);
                DeferredShading::ReloadShaders();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            } else if (Window::IsDirty()) {
                Window::ApplyChanges();
            }
        }
        vkDeviceWaitIdle(vkw::ctx().device);
    }


    bool DirtyFrameResources() {
        bool dirty = false;
        dirty |= vkw::ctx().swapChainDirty;
        dirty |= Window::GetFramebufferResized();
        return dirty;
    }

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = Scene::camera.GetProj() * Scene::camera.GetView() * glm::vec4(position, 1.0f);
        ImVec2 screenSpace = ImVec2(cameraSpace.x / cameraSpace.w, cameraSpace.y / cameraSpace.w);
        const auto ext = vkw::ctx().swapChainExtent;
        screenSpace.x = (screenSpace.x + 1.0) * ext.width/2.0;
        screenSpace.y = (screenSpace.y + 1.0) * ext.height/2.0;
        return screenSpace;
    }

    void imguiDrawFrame() {
        LUZ_PROFILE_FUNC();
        auto device = vkw::ctx().device;
        auto instance = vkw::ctx().instance;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Luz Engine")) {
            if (ImGui::BeginTabBar("LuzEngineMainTab")) {
                if (ImGui::BeginTabItem("Configuration")) {
                    Window::OnImgui();
                    Scene::camera.OnImgui();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Assets")) {
                    AssetManager::OnImgui();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Scene")) {
            Scene::OnImgui();
        }
        ImGui::End();

        if (ImGui::Begin("Inspector")) {
            if (Scene::selectedEntity != nullptr) {
                Scene::InspectEntity(Scene::selectedEntity);
            }
        }
        ImGui::End();

        RayTracing::OnImgui();
        DeferredShading::OnImgui(0);

        ImGui::ShowDemoWindow();

        ImGui::Render();
        imguiDrawData = ImGui::GetDrawData();
    }

    void updateCommandBuffer() {
        LUZ_PROFILE_FUNC();
        auto device = vkw::ctx().device;
        auto instance = vkw::ctx().instance;

        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        auto commandBuffer = vkw::ctx().GetCurrentCommandBuffer();

        vkw::CmdCopy(Scene::sceneBuffer, &Scene::scene, sizeof(Scene::scene));
        vkw::CmdCopy(Scene::modelsBuffer, &Scene::models, sizeof(Scene::models));

        DeferredShading::BeginOpaquePass(commandBuffer);

        DeferredShading::OpaqueConstants constants;
        constants.sceneBufferIndex = Scene::sceneBuffer.rid;
        constants.modelBufferIndex = Scene::modelsBuffer.rid;

        for (Model* model : Scene::modelEntities) {
            constants.modelID = model->id;
            DeferredShading::BindConstants(commandBuffer, DeferredShading::opaquePass, &constants, sizeof(constants));
            DeferredShading::RenderMesh(commandBuffer, model->mesh);
        }

        if (Scene::renderLightGizmos) {
            for (Light* light : Scene::lightEntities) {
                constants.modelID = light->id;
                DeferredShading::BindConstants(commandBuffer, DeferredShading::opaquePass, &constants, sizeof(constants));
                DeferredShading::RenderMesh(commandBuffer, Scene::lightMeshes[light->block.type]);
            }
        }

        DeferredShading::EndPass(commandBuffer);

        DeferredShading::LightConstants lightPassConstants;
        lightPassConstants.sceneBufferIndex = constants.sceneBufferIndex;
        lightPassConstants.frameID = frameCount;
        DeferredShading::LightPass(commandBuffer, lightPassConstants);

        DeferredShading::BeginPresentPass(commandBuffer);
        if (drawUi) {
            ImGui_ImplVulkan_RenderDrawData(imguiDrawData, commandBuffer);
        }
        DeferredShading::EndPresentPass(commandBuffer);
    }

    void drawFrame() {
        LUZ_PROFILE_FUNC();
        imguiDrawFrame();

        auto image = vkw::ctx().Acquire();

        if (vkw::ctx().swapChainDirty) {
            return;
        }

        updateUniformBuffer();
        updateCommandBuffer();

        vkw::ctx().SubmitAndPresent(image);

        frameCount = (frameCount + 1) % (1 << 15);
    }

    void RecreateFrameResources() {
        LUZ_PROFILE_FUNC();
        auto device = vkw::ctx().device;
        //auto instance = vkw::ctx().instance;
        vkDeviceWaitIdle(device);
        // busy wait while the window is minimized
        while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
            Window::WaitEvents();
        }
        Window::UpdateFramebufferSize();
        vkDeviceWaitIdle(device);
        DestroyFrameResources();
        vkw::OnSurfaceUpdate(Window::GetWidth(), Window::GetHeight());
        Scene::CreateResources();
        DeferredShading::Create();
        createUniformProjection();
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = vkw::ctx().swapChainExtent;
        Scene::camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer() {
        LUZ_PROFILE_FUNC();
        Scene::UpdateResources();
    }

    void SetupImgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
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

            colors[ImGuiCol_Text]                 = textColor;
            colors[ImGuiCol_TextDisabled]         = textDisabledColor;
            colors[ImGuiCol_TextSelectedBg]       = panelActiveColor;
            colors[ImGuiCol_WindowBg]             = bgColor;
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
            // colors[ImGuiCol_ModalWindowDarkening] = bgColor;
            colors[ImGuiCol_DragDropTarget]       = bgColor;
            colors[ImGuiCol_NavHighlight]         = bgColor;
            // colors[ImGuiCol_DockingPreview]       = panelActiveColor;
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
        auto device = vkw::ctx().device;
        auto instance = vkw::ctx().instance;

        ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWwindow(), true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = vkw::ctx().physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = vkw::ctx().queues[vkw::Queue::Graphics].family;
        initInfo.Queue = vkw::ctx().queues[vkw::Queue::Graphics].queue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = GraphicsPipelineManager::GetImguiDescriptorPool();
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = (uint32_t)vkw::ctx().swapChainImages.size();
        initInfo.MSAASamples = vkw::ctx().numSamples;
        initInfo.Allocator = VK_NULL_HANDLE;
        initInfo.CheckVkResultFn = CheckVulkanResult;
        initInfo.UseDynamicRendering = true;
        initInfo.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
        ImGui_ImplVulkan_Init(&initInfo, nullptr);
        ImGui_ImplVulkan_CreateFontsTexture();
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
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}