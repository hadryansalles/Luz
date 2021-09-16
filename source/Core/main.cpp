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
#include "UnlitGraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "BufferManager.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "AssetManager.hpp"

#include <stb_image.h>

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
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

class VulkanTutorialApplication {
public:
    void run() {
        WaitToInit(4);
        Setup();
        Create();
        MainLoop();
        Finish();
    }

private:
    SceneUBO sceneUBO;

    // imgui
    ImDrawData* imguiDrawData = nullptr;

    // camera 
    Camera camera;

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
        UnlitGraphicsPipeline::Setup();
        TextureManager::Setup();
        SetupImgui();
    }

    void Create() {
        LUZ_PROFILE_FUNC();
        CreateVulkan();
        SceneManager::Setup();
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
        UnlitGraphicsPipeline::Create();
        CreateImgui();
        createUniformProjection();
        TextureManager::Create();
        TextureManager::CreateImguiTextureDescriptors();
        MeshManager::Create();
        SceneManager::Create();
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        DestroyVulkan();
        SceneManager::Finish();
        MeshManager::Finish();
        TextureManager::Finish();
        FinishImgui();
    }

    void DestroyVulkan() {
        LUZ_PROFILE_FUNC();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        DestroyFrameResources();

        MeshManager::Destroy();
        TextureManager::Destroy();
        LogicalDevice::Destroy();
        PhysicalDevice::Destroy();
        Instance::Destroy();
        Window::Destroy();
    }

    void DestroyFrameResources() {
        LUZ_PROFILE_FUNC();
        SceneManager::Destroy();
        UnlitGraphicsPipeline::Destroy();
        GraphicsPipelineManager::Destroy();

        DestroyImgui();

        SwapChain::Destroy();
        LOG_INFO("Destroyed SwapChain.");
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            Window::Update();
            SceneManager::Update();
            camera.Update();
            drawFrame();
            if (DirtyGlobalResources()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                DestroyVulkan();
                CreateVulkan();
            } else if (DirtyFrameResources()) {
                RecreateFrameResources();
            } else if (UnlitGraphicsPipeline::IsDirty()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                UnlitGraphicsPipeline::Destroy();
                UnlitGraphicsPipeline::Create();
            } else if (Window::IsDirty()) {
                Window::ApplyChanges();
            }
        }
        vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
    }

    bool DirtyGlobalResources() {
        bool dirty = false;
        dirty |= Instance::IsDirty();
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

    void imguiDrawFrame() {
        LUZ_PROFILE_FUNC();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    
        if (ImGui::Begin("Vulkan")) {
            Window::OnImgui();
            Instance::OnImgui();
            PhysicalDevice::OnImgui();
            LogicalDevice::OnImgui();
            SwapChain::OnImgui();
            UnlitGraphicsPipeline::OnImgui();
            camera.OnImgui();
        }
        ImGui::End();

        if (ImGui::Begin("Scene")) {
            SceneManager::OnImgui();
            MeshManager::OnImgui();
            TextureManager::OnImgui();
        }
        ImGui::End();

        if (ImGui::Begin("Inspector")) {

            Transform* selectedTransform = SceneManager::GetSelectedTransform();

            if (selectedTransform != nullptr) {
                Transform& transform = *selectedTransform;
                ImGuizmo::BeginFrame();
                static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
                static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

                if (ImGui::CollapsingHeader("Transform")) {
                    if (ImGui::IsKeyPressed(GLFW_KEY_1)) {
                        currentGizmoOperation = ImGuizmo::TRANSLATE;
                    }
                    if (ImGui::IsKeyPressed(GLFW_KEY_2)) {
                        currentGizmoOperation = ImGuizmo::ROTATE;
                    }
                    if (ImGui::IsKeyPressed(GLFW_KEY_3)) {
                        currentGizmoOperation = ImGuizmo::SCALE;
                    }
                    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
                        currentGizmoOperation = ImGuizmo::TRANSLATE;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
                        currentGizmoOperation = ImGuizmo::ROTATE;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
                        currentGizmoOperation = ImGuizmo::SCALE;
                    }

                    ImGui::InputFloat3("Position", glm::value_ptr(transform.position));
                    ImGui::InputFloat3("Rotation", glm::value_ptr(transform.rotation));
                    ImGui::InputFloat3("Scale", glm::value_ptr(transform.scale));

                    if (currentGizmoOperation != ImGuizmo::SCALE) {
                        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL)) {
                            currentGizmoMode = ImGuizmo::LOCAL;
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD)) {
                            currentGizmoMode = ImGuizmo::WORLD;
                        }
                    } else {
                        currentGizmoMode = ImGuizmo::LOCAL;
                    }
                }
                glm::mat4 modelMat = transform.GetMatrix();
                // ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform.position), glm::value_ptr(transform.rotation),
                //     glm::value_ptr(transform.scale), glm::value_ptr(transform.transform));

                glm::mat4 guizmoProj(sceneUBO.proj);
                guizmoProj[1][1] *= -1;

                ImGuiIO& io = ImGui::GetIO();
                ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
                ImGuizmo::Manipulate(glm::value_ptr(sceneUBO.view), glm::value_ptr(guizmoProj), currentGizmoOperation,
                currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

                if (transform.parent != nullptr) {
                    modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
                }
                transform.transform = modelMat;
                
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.transform), glm::value_ptr(transform.position), 
                    glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
            }

            Model* selectedModel = SceneManager::GetSelectedModel();
            if (selectedModel != nullptr) {
                if (ImGui::CollapsingHeader("Texture")) {
                    if (ImGui::BeginDragDropTarget()) {
                        const ImGuiPayload* texturePayload = ImGui::AcceptDragDropPayload("texture");
                        if (texturePayload) {
                            std::string texturePath((const char*)texturePayload->Data, texturePayload->DataSize);
                            SceneManager::LoadAndSetTexture(selectedModel, texturePath);
                            ImGui::EndDragDropTarget();
                        }
                    }
                    TextureManager::DrawOnImgui(selectedModel->texture);
                }
            }
        }
        ImGui::End();

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

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = SwapChain::GetRenderPass();
        renderPassInfo.framebuffer = SwapChain::GetFramebuffer(frameIndex);

        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = SwapChain::GetExtent();

        std::array<VkClearValue, 2> clearValues{}; 
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        auto sceneDescriptor = SceneManager::GetSceneDescriptor();
        auto unlitGPO = UnlitGraphicsPipeline::GetResource();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.layout, 0,
                    1, &sceneDescriptor.descriptors[frameIndex], 0, nullptr);

        for (const Model* model : SceneManager::GetModels()) {
            if (model->mesh != nullptr) {
                MeshResource* mesh = model->mesh;
                VkBuffer vertexBuffers[] = { mesh->vertexBuffer.buffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                // command buffer, vertex count, instance count, first vertex, first instance
                // vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.layout, 1,
                    1, &model->meshDescriptor.descriptors[frameIndex], 0, nullptr);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.layout, 2,
                    1, &model->materialDescriptor.descriptors[frameIndex], 0, nullptr);
                vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1, 0, 0, 0);
            }
        }

        ImGui_ImplVulkan_RenderDrawData(imguiDrawData, commandBuffer);

        vkCmdEndRenderPass(commandBuffer);

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
        GraphicsPipelineManager::Create();
        UnlitGraphicsPipeline::Create();
        SceneManager::Create();
        CreateImgui();
        createUniformProjection();
        TextureManager::CreateImguiTextureDescriptors();
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = SwapChain::GetExtent();
        camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        LUZ_PROFILE_FUNC();
        for (Model* model : SceneManager::GetModels()) {
            model->ubo.model = model->transform.GetMatrix();
            BufferManager::Update(model->meshDescriptor.buffers[currentImage], &model->ubo, sizeof(model->ubo));
        }

        sceneUBO.view = camera.GetView();
        sceneUBO.proj = camera.GetProj();
        BufferManager::Update(SceneManager::GetSceneDescriptor().buffers[currentImage], &sceneUBO, sizeof(sceneUBO));
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
    VulkanTutorialApplication app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}