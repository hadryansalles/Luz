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
#include "PhongGraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "BufferManager.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "AssetManager.hpp"
#include "MaterialManager.hpp"

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

#define MAX_TEXTURES 4096
#define MAX_MODELS 4096
#define MAX_LIGHTS_PER_TYPE 8

#define SCENE_BUFFER_INDEX2 0
#define MODELS_BUFFER_INDEX2 1

struct LightBlock {
    glm::vec3 color = glm::vec3(1.0f);
    f32 intensity = 1.0f;
    glm::vec3 position = glm::vec3(1.0f);
    f32 innerAngle = 0;
    glm::vec3 direction = glm::vec3(.0f, .0f, 1.0f);
    f32 outerAngle = 0;
};

struct SceneBlock {
    LightBlock pointLights[MAX_LIGHTS_PER_TYPE];
    LightBlock spotLights[MAX_LIGHTS_PER_TYPE];
    LightBlock dirLights[MAX_LIGHTS_PER_TYPE];
    glm::vec3 ambientLightColor = glm::vec3(1.0f);
    f32 ambientLightIntensity = 0.1f;
    glm::mat4 projView = glm::mat4(1.0f);
    glm::vec3 camPos = glm::vec3(.0f, .0f, .0f);
    u32 numPointLights = 0;
    u32 numSpotLights = 0;
    u32 numDirLights = 0;
    f32 PADDING[2];
};

struct ModelBlock {
    glm::mat4 model     = glm::mat4(1.0f);
    glm::vec4 colors[2] = { glm::vec4(1.0f), glm::vec4(1.0f) };
    f32 values[2]       = { .0f, .0f };
    RID textures[2]     = { 0, 0 };
};

struct ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

enum EntityType {
    InvalidEntity,
    ModelEntity,
    CollectionEntity,
    LightEntity
};

struct Transform2 {
    glm::vec3 position = glm::vec3(.0f);
    glm::vec3 rotation = glm::vec3(.0f);
    glm::vec3 scale    = glm::vec3(1.0f);
};

struct Entity {
    std::string name = "Entity";
    Transform transform;
    EntityType entityType = EntityType::InvalidEntity;
};

enum LightType2 {
    Point2,
    Directional2,
    Spot2
};

enum Material2 {
    Phong2,
    Unlit2
};

struct Model2 : Entity {
    RID meshID = 0;
    Material2 material = Material2::Phong2;
};

struct Light2 : Entity {
    LightType2 lightType = LightType2::Point2;
};

struct SceneResource {
    SceneBlock scene;
    ModelBlock models[MAX_MODELS];
    RID textures[MAX_TEXTURES];
    StorageBuffer sceneBuffer;
    StorageBuffer modelsBuffer;
    std::vector<RID> freeTextureRIDs;
    std::vector<RID> freeModelRIDs; 

    Camera camera;
    std::vector<Entity*> entities;
    int selectedEntity = -1;

    bool renderLightGizmos = true;
    float lightGizmosOpacity = 1.0f;
};

Model2* CreateModel(SceneResource& res, std::string name, RID meshID) {
    Model2* model = new Model2();
    model->name = name;
    model->meshID = meshID;
    model->entityType = EntityType::ModelEntity;
    res.entities.push_back(model);
    return model;
}

Light2* CreateLight(SceneResource& res, std::string name, LightType2 type) {
    Light2* light = new Light2();
    light->name = name;
    light->entityType = EntityType::LightEntity;
    light->lightType = type;
    res.entities.push_back(light);
    return light;
}

void CreateSceneBuffers(SceneResource& res) {
    BufferManager::CreateStorageBuffer(res.sceneBuffer, sizeof(res.scene));
    BufferManager::CreateStorageBuffer(res.modelsBuffer, sizeof(res.models));
    GraphicsPipelineManager::WriteStorage(res.sceneBuffer, SCENE_BUFFER_INDEX2);
    GraphicsPipelineManager::WriteStorage(res.modelsBuffer, MODELS_BUFFER_INDEX2);
}

void DestroySceneBuffers(SceneResource& res) {
    BufferManager::DestroyStorageBuffer(res.sceneBuffer);
    BufferManager::DestroyStorageBuffer(res.modelsBuffer);
}

void UpdateSceneResource(SceneResource& res, int numFrame) {
    LUZ_PROFILE_FUNC();
    res.scene.projView = res.camera.GetProj() * res.camera.GetView();
    BufferManager::UpdateStorage(res.sceneBuffer, numFrame, &res.scene);
    BufferManager::UpdateStorage(res.modelsBuffer, numFrame, &res.models);
}

void OnImgui(SceneResource& res) {
    if (ImGui::CollapsingHeader("Scene Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < res.entities.size(); i++) {
            ImGui::PushID(i);
            if (ImGui::Selectable(res.entities[i]->name.c_str(), res.selectedEntity == i)) {
                res.selectedEntity = i;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add model")) {
            CreateModel(res, "New Model", 0);
        }
        if (ImGui::Button("Add light")) {
            CreateLight(res, "New Light", LightType2::Point2);
        }
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(res.scene.ambientLightColor));
        ImGui::DragFloat("Ambient intensity", &res.scene.ambientLightIntensity);
    }
}

void InspectModel(SceneResource& res, int id, Model2* model) {
    ImGui::Text("Mesh: %d", model->meshID);
}

void RenderTransformGizmos(SceneResource& res, int id) {
    Transform& transform = res.entities[id]->transform;

    ImGuizmo::BeginFrame();
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    glm::mat4 guizmoProj(res.camera.GetProj());
    guizmoProj[1][1] *= -1;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(res.camera.GetView()), glm::value_ptr(guizmoProj), currentGizmoOperation,
    currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

    if (transform.parent != nullptr) {
        modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
    }
    transform.transform = modelMat;
    
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.transform), glm::value_ptr(transform.position), 
        glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
}

void InspectEntity(SceneResource& res, int id) {
    Entity* entity = res.entities[id];
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", glm::value_ptr(entity->transform.position));
        ImGui::DragFloat3("Scale", glm::value_ptr(entity->transform.scale));
        ImGui::DragFloat3("Rotation", glm::value_ptr(entity->transform.rotation));
        RenderTransformGizmos(res, id);
    }
    if (entity->entityType == EntityType::ModelEntity) {
        InspectModel(res, id, (Model2*)entity);
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
    SceneUBO sceneUBO;
    UniformBuffer sceneUniform;
    ImDrawData* imguiDrawData = nullptr;
    SceneResource sceneResource;

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
        PhongGraphicsPipeline::Setup();
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
        PhongGraphicsPipeline::Create();
        CreateImgui();
        createUniformProjection();
        TextureManager::Create();
        MeshManager::Create();
        SceneManager::Create();
        LightManager::Create();
        BufferManager::CreateUniformBuffer(sceneUniform, sizeof(sceneUBO));
        GraphicsPipelineManager::WriteUniform(sceneUniform, GraphicsPipelineManager::SCENE_BUFFER_INDEX);
        CreateSceneBuffers(sceneResource);
    }

    void Finish() {
        LUZ_PROFILE_FUNC();
        DestroyVulkan();
        LightManager::Finish();
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
        GraphicsPipelineManager::Destroy();
        MeshManager::Destroy();
        TextureManager::Destroy();
        LogicalDevice::Destroy();
        PhysicalDevice::Destroy();
        Instance::Destroy();
        Window::Destroy();
    }

    void DestroyFrameResources() {
        LUZ_PROFILE_FUNC();
        LightManager::Destroy();
        SceneManager::Destroy();
        UnlitGraphicsPipeline::Destroy();
        PhongGraphicsPipeline::Destroy();
        BufferManager::DestroyUniformBuffer(sceneUniform);
        DestroySceneBuffers(sceneResource);
        DestroyImgui();

        SwapChain::Destroy();
        LOG_INFO("Destroyed SwapChain.");
    }

    void MainLoop() {
        while (!Window::GetShouldClose()) {
            LUZ_PROFILE_FRAME();
            Window::Update();
            SceneManager::Update();
            sceneResource.camera.Update();
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
            } else if (PhongGraphicsPipeline::IsDirty()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                PhongGraphicsPipeline::Destroy();
                PhongGraphicsPipeline::Create();
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

    ImVec2 ToScreenSpace(glm::vec3 position) {
        glm::vec4 cameraSpace = sceneResource.camera.GetProj() * sceneResource.camera.GetView() * glm::vec4(position, 1.0f);
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
                    UnlitGraphicsPipeline::OnImgui();
                    PhongGraphicsPipeline::OnImgui();
                    sceneResource.camera.OnImgui();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Assets")) {
                    AssetManager::OnImgui();
                    MeshManager::OnImgui();
                    TextureManager::OnImgui();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Scene")) {
            SceneManager::OnImgui();
            LightManager::OnImgui();
            OnImgui(sceneResource);
        }
        ImGui::End();

        if (ImGui::Begin("Inspector")) {
            // const float totalWidth = ImGui::GetContentRegionAvailWidth();

            // Collection* selectedCollection = SceneManager::GetSelectedCollection();
            // Model* selectedModel = SceneManager::GetSelectedModel();
            // Light* selectedLight = SceneManager::GetSelectedLight();
            // Transform* selectedTransform = SceneManager::GetSelectedTransform();

            // if (selectedCollection != nullptr) {
            //     ImGui::Text("Name");
            //     ImGui::SameLine(totalWidth/5.0, -1);
            //     ImGui::SetNextItemWidth(totalWidth * 4.0 / 5.0);
            //     ImGui::PushID("name");
            //     ImGui::InputText("", &selectedCollection->name);
            //     ImGui::PopID();
            // }

            // if (selectedModel != nullptr) {
            //     ImGui::Text("Name");
            //     ImGui::SameLine(totalWidth/5.0, -1);
            //     ImGui::SetNextItemWidth(totalWidth * 4.0 / 5.0);
            //     ImGui::PushID("name");
            //     ImGui::InputText("", &selectedModel->name);
            //     ImGui::PopID();
            // }
            // 
            // if (selectedLight != nullptr) {
            //     ImGui::Text("Name");
            //     ImGui::SameLine(totalWidth/5.0, -1);
            //     ImGui::SetNextItemWidth(totalWidth * 4.0 / 5.0);
            //     ImGui::PushID("name");
            //     ImGui::InputText("", &selectedLight->name);
            //     ImGui::PopID();
            // }

            // if (selectedTransform != nullptr) {
            //     Transform& transform = *selectedTransform;
            //     ImGuizmo::BeginFrame();
            //     static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
            //     static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

            //     if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //  {
             //       if (ImGui::IsKeyPressed(GLFW_KEY_1)) {
             //           currentGizmoOperation = ImGuizmo::TRANSLATE;
             //       }
             //       if (ImGui::IsKeyPressed(GLFW_KEY_2)) {
             //           currentGizmoOperation = ImGuizmo::ROTATE;
             //       }
             //       if (ImGui::IsKeyPressed(GLFW_KEY_3)) {
             //           currentGizmoOperation = ImGuizmo::SCALE;
             //       }
             //       if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
             //           currentGizmoOperation = ImGuizmo::TRANSLATE;
             //       }
             //       ImGui::SameLine();
             //       if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
             //           currentGizmoOperation = ImGuizmo::ROTATE;
             //       }
             //       ImGui::SameLine();
             //       if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
             //           currentGizmoOperation = ImGuizmo::SCALE;
             //       }

             //       ImGui::InputFloat3("Position", glm::value_ptr(transform.position));
             //       ImGui::InputFloat3("Rotation", glm::value_ptr(transform.rotation));
             //       ImGui::InputFloat3("Scale", glm::value_ptr(transform.scale));

             //       if (currentGizmoOperation != ImGuizmo::SCALE) {
             //           if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL)) {
             //               currentGizmoMode = ImGuizmo::LOCAL;
             //           }
             //           ImGui::SameLine();
             //           if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD)) {
             //               currentGizmoMode = ImGuizmo::WORLD;
             //           }
             //       } else {
             //           currentGizmoMode = ImGuizmo::LOCAL;
             //       }
             //   }
            //     glm::mat4 modelMat = transform.GetMatrix();
            //     // ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform.position), glm::value_ptr(transform.rotation),
            //     //     glm::value_ptr(transform.scale), glm::value_ptr(transform.transform));

            //     glm::mat4 guizmoProj(sceneUBO.proj);
            //     guizmoProj[1][1] *= -1;

            //     ImGuiIO& io = ImGui::GetIO();
            //     ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
            //     ImGuizmo::Manipulate(glm::value_ptr(sceneUBO.view), glm::value_ptr(guizmoProj), currentGizmoOperation,
            //     currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

            //     if (transform.parent != nullptr) {
            //         modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
            //     }
            //     transform.transform = modelMat;
            //     
            //     ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.transform), glm::value_ptr(transform.position), 
            //         glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
            // }

            // if (selectedModel != nullptr) {
            //     MaterialManager::OnImgui(selectedModel);
            // }
            // if (selectedLight) {
            //     LightManager::OnImgui(selectedLight);
            //     LightManager::SetDirty();
            // }

            // if (selectedCollection) {
            //     LightManager::SetDirty();
            // }

            if (sceneResource.selectedEntity != -1) {
                InspectEntity(sceneResource, sceneResource.selectedEntity);
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

        ConstantsBlock constants;
        constants.sceneBufferIndex = SwapChain::GetNumFrames() * SCENE_BUFFER_INDEX2 + frameIndex;
        constants.modelBufferIndex = SwapChain::GetNumFrames() * MODELS_BUFFER_INDEX2 + frameIndex;

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

        auto unlitGPO = UnlitGraphicsPipeline::GetResource();
        auto phongGPO = PhongGraphicsPipeline::GetResource();
        auto BIND_GRAPHICS = VK_PIPELINE_BIND_POINT_GRAPHICS;
        auto& descriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, phongGPO.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, BIND_GRAPHICS, phongGPO.layout, 0, 1, &descriptorSet, 0, nullptr);

        for (int i = 0; i < sceneResource.entities.size(); i++) {
            Entity* entity = sceneResource.entities[i];
            if (entity->entityType == EntityType::ModelEntity) {
                Model2* model = (Model2*)entity;
                MeshResource* mesh = MeshManager::GetMesh(model->meshID);
                VkBuffer vertexBuffers[] = { mesh->vertexBuffer.buffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                constants.modelID = i;
                vkCmdPushConstants(commandBuffer, phongGPO.layout, VK_SHADER_STAGE_ALL, 0, sizeof(ConstantsBlock), &constants);
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
        UnlitGraphicsPipeline::Create();
        PhongGraphicsPipeline::Create();
        SceneManager::Create();
        LightManager::Create();
        BufferManager::CreateUniformBuffer(sceneUniform, sizeof(sceneUBO));
        CreateSceneBuffers(sceneResource);
        GraphicsPipelineManager::WriteUniform(sceneUniform, GraphicsPipelineManager::SCENE_BUFFER_INDEX);
        CreateImgui();
        createUniformProjection();
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = SwapChain::GetExtent();
        sceneResource.camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        LUZ_PROFILE_FUNC();
        LightManager::UpdateBufferIfDirty(currentImage);
        BufferManager::SetDirtyUniform(sceneUniform);
        sceneUBO.view = sceneResource.camera.GetView();
        sceneUBO.proj = sceneResource.camera.GetProj();
        BufferManager::UpdateUniformIfDirty(sceneUniform, currentImage, &sceneUBO);
        // auto models = SceneManager::GetModels();
        // for (int i = 0; i < models.size(); i++) {
        //     sceneResource.models[i].model = models[i]->transform.GetMatrix();
        //     sceneResource.models[i].colors[0] = models[i]->material.diffuseColor;
        //     sceneResource.models[i].textures[0] = models[i]->material.diffuseTexture;
        // }
        // auto lights = LightManager::GetLights();
        // for (int i = 0; i < lights.size(); i++) {
        //     int lightID = models.size() + i;
        //     sceneResource.models[lightID].model = lights[i]->model->transform.GetMatrix();
        //     sceneResource.models[lightID].colors[0] = lights[i]->color;
        //     sceneResource.models[lightID].textures[0] = 0;
        // }
        sceneResource.scene.numDirLights = 0;
        sceneResource.scene.numSpotLights = 0;
        sceneResource.scene.numPointLights = 0;
        for (int i = 0; i < sceneResource.entities.size(); i++) {
            Entity* entity = sceneResource.entities[i];
            if (entity->entityType == EntityType::ModelEntity) {
                sceneResource.models[i].model = entity->transform.GetMatrix();
                sceneResource.models[i].colors[0] = glm::vec4(1.0f);
                sceneResource.models[i].textures[0] = 0;
            } if (entity->entityType == EntityType::LightEntity) {
                sceneResource.models[i].model = entity->transform.GetMatrix();
                Light2* light = (Light2*)entity;
                if (light->lightType == LightType2::Directional2) {
                    sceneResource.scene.numDirLights++;
                }
                if (light->lightType == LightType2::Spot2) {
                    sceneResource.scene.numSpotLights++;
                }
                if (light->lightType == LightType2::Point2) {
                    sceneResource.scene.pointLights[sceneResource.scene.numPointLights].position = entity->transform.position;
                    sceneResource.scene.numPointLights++;
                }
            }
        }
        UpdateSceneResource(sceneResource, currentImage);
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