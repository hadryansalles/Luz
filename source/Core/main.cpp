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
        DEBUG_TRACE("Finish loading model.");
        CreateImgui();
        createUniformProjection();
        DEBUG_TRACE("Finish initializing Vulkan.");
        TextureManager::Create();
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
        }
        ImGui::End();

        ImGuizmo::BeginFrame();
        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
        static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

        Model* selectedModel = SceneManager::GetSelectedModel();

        if (selectedModel != nullptr) {
            if (ImGui::Begin("Transform")) {
                Transform& transform = selectedModel->transform;
                ModelUBO& modelUBO = selectedModel->ubo;
     
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
                ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform.position), glm::value_ptr(transform.rotation), 
                    glm::value_ptr(transform.scale), glm::value_ptr(modelUBO.model));

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

                glm::mat4 guizmoProj(sceneUBO.proj);
                guizmoProj[1][1] *= -1;

                ImGuiIO& io = ImGui::GetIO();
                ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
                ImGuizmo::Manipulate(glm::value_ptr(sceneUBO.view), glm::value_ptr(guizmoProj), currentGizmoOperation,
                    currentGizmoMode, glm::value_ptr(modelUBO.model), nullptr, nullptr);
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelUBO.model), glm::value_ptr(transform.position), 
                    glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
            }
            ImGui::End();
        }

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
        ImGui::StyleColorsDark();
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