#include "Luzpch.hpp" 

#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "Instance.hpp"
#include "ImageManager.hpp"
#include "Window.hpp"
#include "SwapChain.hpp"
#include "Camera.hpp"
#include "Shader.hpp"
#include "GraphicsPipelineManager.hpp"
#include "PBRGraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "BufferManager.hpp"
#include "AssetManager.hpp"
#include "Scene.hpp"
#include "RayTracing.hpp"
#include "DeferredRenderer.hpp"

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
        PBRGraphicsPipeline::Setup();
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
        Instance::Create();
        PhysicalDevice::Create();
        LogicalDevice::Create();
        SwapChain::Create();
        DEBUG_TRACE("Finish creating SwapChain.");
        GraphicsPipelineManager::Create();
        PBRGraphicsPipeline::Create();
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
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        DestroyFrameResources();
        RayTracing::Destroy();
        GraphicsPipelineManager::Destroy();
        AssetManager::Destroy();
        LogicalDevice::Destroy();
        PhysicalDevice::Destroy();
        Instance::Destroy();
        Window::Destroy();
    }

    void DestroyFrameResources() {
        LUZ_PROFILE_FUNC();
        PBRGraphicsPipeline::Destroy();
        Scene::DestroyResources();
        
        DestroyImgui();
        DeferredShading::Destroy();

        SwapChain::Destroy();
        LOG_INFO("Destroyed SwapChain.");
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
            if (Window::IsKeyPressed(GLFW_KEY_R)) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                DeferredShading::ReloadShaders();
            }
            if (DirtyGlobalResources()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                DestroyVulkan();
                CreateVulkan();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            } else if (PBRGraphicsPipeline::IsDirty()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                PBRGraphicsPipeline::Destroy();
                PBRGraphicsPipeline::Create();
            } else if (Window::IsDirty()) {
                Window::ApplyChanges();
            }
        }
        vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
    }

    bool DirtyGlobalResources() {
        bool dirty = false;
        // dirty |= Instance::IsDirty();
        dirty |= PhysicalDevice::IsDirty();
        dirty |= LogicalDevice::IsDirty();
        return dirty;
    }

    bool DirtyFrameResources() {
        bool dirty = false;
        dirty |= SwapChain::IsDirty();
        dirty |= Window::GetFramebufferResized();
        return dirty;
    }

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = Scene::camera.GetProj() * Scene::camera.GetView() * glm::vec4(position, 1.0f);
        ImVec2 screenSpace = ImVec2(cameraSpace.x / cameraSpace.w, cameraSpace.y / cameraSpace.w);
        auto ext = SwapChain::GetExtent();
        screenSpace.x = (screenSpace.x + 1.0) * ext.width/2.0;
        screenSpace.y = (screenSpace.y + 1.0) * ext.height/2.0;
        return screenSpace;
    }

    void imguiDrawFrame() {
        LUZ_PROFILE_FUNC();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Luz Engine")) {
            if (ImGui::BeginTabBar("LuzEngineMainTab")) {
                if (ImGui::BeginTabItem("Configuration")) {
                    Window::OnImgui();
                    Instance::OnImgui();
                    PhysicalDevice::OnImgui();
                    LogicalDevice::OnImgui();
                    SwapChain::OnImgui();
                    PBRGraphicsPipeline::OnImgui();
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

    void updateCommandBuffer(size_t frameIndex) {
        LUZ_PROFILE_FUNC();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        auto commandBuffer = SwapChain::GetCommandBuffer(frameIndex);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        DeferredShading::BeginOpaquePass(commandBuffer);

        DeferredShading::OpaqueConstants constants;
        constants.sceneBufferIndex = SwapChain::GetNumFrames() * SCENE_BUFFER_INDEX + frameIndex;
        constants.modelBufferIndex = SwapChain::GetNumFrames() * MODELS_BUFFER_INDEX + frameIndex;

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

        DeferredShading::BeginPresentPass(commandBuffer, frameIndex);
        ImGui_ImplVulkan_RenderDrawData(imguiDrawData, commandBuffer);
        DeferredShading::EndPresentPass(commandBuffer, frameIndex);

        // vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void drawFrame() {
        LUZ_PROFILE_FUNC();
        imguiDrawFrame();

        auto image = SwapChain::Acquire(); 

        if (SwapChain::IsDirty()) {
            return;
        }

        updateUniformBuffer(image);
        updateCommandBuffer(image);

        SwapChain::SubmitAndPresent(image);

        frameCount = (frameCount + 1) % (1 << 15);
    }

    void RecreateFrameResources() {
        LUZ_PROFILE_FUNC();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        vkDeviceWaitIdle(device);
        // busy wait while the window is minimized
        while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
            Window::WaitEvents();
        }
        Window::UpdateFramebufferSize();
        vkDeviceWaitIdle(device);
        DestroyFrameResources();
        PhysicalDevice::OnSurfaceUpdate();
        SwapChain::Create();
        PBRGraphicsPipeline::Create();
        Scene::CreateResources();
        CreateImgui();
        DeferredShading::Create();
        createUniformProjection();
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = SwapChain::GetExtent();
        Scene::camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        LUZ_PROFILE_FUNC();
        Scene::UpdateResources(currentImage);
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
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWwindow(), true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = PhysicalDevice::GetVkPhysicalDevice();
        initInfo.Device = device;
        initInfo.QueueFamily = PhysicalDevice::GetGraphicsFamily();
        initInfo.Queue = LogicalDevice::GetGraphicsQueue();
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = GraphicsPipelineManager::GetImguiDescriptorPool();
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = (uint32_t)SwapChain::GetNumFrames();
        initInfo.MSAASamples = SwapChain::GetNumSamples();
        initInfo.Allocator = VK_NULL_HANDLE;
        initInfo.CheckVkResultFn = CheckVulkanResult;
        ImGui_ImplVulkan_Init(&initInfo, SwapChain::GetRenderPass());
        
        auto commandBuffer = LogicalDevice::BeginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        LogicalDevice::EndSingleTimeCommands(commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
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