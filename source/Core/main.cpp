#include "Luzpch.hpp" 

#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "Instance.hpp"
#include "Image.hpp"
#include "Window.hpp"
#include "SwapChain.hpp"
#include "Camera.hpp"
#include "Shader.hpp"
#include "GraphicsPipeline.hpp"
#include "FileManager.hpp"
#include "Buffers.hpp"
#include "Scene.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tiny_obj_loader.h>

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

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

    static VkVertexInputBindingDescription getBindingDescription() {
        // specifies at which rate load data from memory throughout the vertices,
        // number of bytes between data entries 
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        // describe how to extract each vertex attribute from the chunk of data
        // inside the binding description
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(Vertex, pos);;

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset   = offsetof(Vertex, color);;

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);;

        return attributeDescriptions;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

// vulkan always expect data to be alligned with multiples of 16
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Transform {
    glm::vec3 position = glm::vec3(.0f);
    glm::vec3 rotation = glm::vec3(.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getMatrix() {
        glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rotation)));
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 scaleMat = glm::scale(scale);
        return translationMat * scaleMat * rotationMat;
    }
};

class VulkanTutorialApplication {
public:
    void run() {
        firstInitImgui();
        Scene::Init();
        setupUnlitPipeline();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    const std::string MODEL_PATH = "assets/models/viking_room.obj";
    const std::string TEXTURE_PATH = "assets/models/viking_room.png";

    // vulkan members
    GraphicsPipelineResource unlitPipeline;
    GraphicsPipelineDesc unlitPipelineDesc;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    int MAX_FRAMES_IN_FLIGHT = -1;
    size_t currentFrame = 0;
    bool framebufferResized = false;

    // vertex buffers
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BufferResource vertexBuffer;
    BufferResource indexBuffer;

    // image 
    uint32_t mipLevels;
    VkImage textureImage;
    VkSampler textureSampler;
    VkImageView textureImageView;
    VkDeviceMemory textureImageMemory;

    std::vector<BufferResource> uniformBuffers;

    // imgui
    ImDrawData* imguiDrawData = nullptr;

    // imguizmo

    // uniform
    UniformBufferObject ubo;
    Transform transform;

    // camera 
    Camera camera;

    void initVulkan() {
        Window::Create();
        currentFrame = 0;
        Instance::Create();
        PhysicalDevice::Create();
        LogicalDevice::Create();
        SwapChain::Create();
        MAX_FRAMES_IN_FLIGHT = SwapChain::GetFramesInFlight();
        DEBUG_TRACE("Finish creating SwapChain.");
        createGraphicsPipeline();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        loadModel();
        DEBUG_TRACE("Finish loading model.");
        Buffers::CreateVertexBuffer(vertexBuffer, vertices.data(), sizeof(vertices[0]) * vertices.size());
        Buffers::CreateIndexBuffer(indexBuffer, indices.data(), sizeof(indices[0]) * indices.size());
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
        initImgui();
        createUniformModelView();
        createUniformProjection();
        DEBUG_TRACE("Finish initializing Vulkan.");
    }
    
    void cleanup() {
        cleanupVulkan();
        finishImgui();
    }

    void cleanupVulkan() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        cleanupSwapChain();

        Buffers::Destroy(vertexBuffer);
        Buffers::Destroy(indexBuffer);

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        LogicalDevice::Destroy();
        PhysicalDevice::Destroy();
        Instance::Destroy();    

        uniformBuffers.clear();

        Window::Destroy();
    }

    void cleanupSwapChain() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        vkFreeCommandBuffers(device, LogicalDevice::GetCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        GraphicsPipeline::Destroy(unlitPipeline);

        for (size_t i = 0; i < SwapChain::GetNumFrames(); i++) {
            Buffers::Destroy(uniformBuffers[i]);
            // vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            // vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        cleanupImgui();

        SwapChain::Destroy();
        LOG_INFO("Destroyed SwapChain.");

        commandBuffers.clear();
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();
        imagesInFlight.clear();
    }

    void mainLoop() {
        auto instance = Instance::GetVkInstance();
        while (!Window::GetShouldClose()) {
            Window::Update();
            camera.Update();
            drawFrame();
            if (Instance::IsDirty() || PhysicalDevice::IsDirty() || Window::IsDirty()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                cleanupVulkan();
                initVulkan();
            }
        }
        vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
    }

    void setupUnlitPipeline() {
        unlitPipelineDesc.name = "Unlit";

        unlitPipelineDesc.shaderStages.resize(2);
        unlitPipelineDesc.shaderStages[0].shaderBytes = FileManager::ReadRawBytes("bin/shader.vert.spv");
        unlitPipelineDesc.shaderStages[0].stageBit = VK_SHADER_STAGE_VERTEX_BIT;
        unlitPipelineDesc.shaderStages[1].shaderBytes = FileManager::ReadRawBytes("bin/shader.frag.spv");
        unlitPipelineDesc.shaderStages[1].stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;

        unlitPipelineDesc.bindingDesc = Vertex::getBindingDescription();
        unlitPipelineDesc.attributesDesc = Vertex::getAttributeDescriptions();

        unlitPipelineDesc.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // fragments beyond near and far planes are clamped to them
        unlitPipelineDesc.rasterizer.depthClampEnable = VK_FALSE;
        unlitPipelineDesc.rasterizer.rasterizerDiscardEnable = VK_FALSE;
        unlitPipelineDesc.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // line thickness in terms of number of fragments
        unlitPipelineDesc.rasterizer.lineWidth = 1.0f;
        unlitPipelineDesc.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        unlitPipelineDesc.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        unlitPipelineDesc.rasterizer.depthBiasEnable = VK_FALSE;
        unlitPipelineDesc.rasterizer.depthBiasConstantFactor = 0.0f;
        unlitPipelineDesc.rasterizer.depthBiasClamp = 0.0f;
        unlitPipelineDesc.rasterizer.depthBiasSlopeFactor = 0.0f;

        unlitPipelineDesc.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        unlitPipelineDesc.multisampling.rasterizationSamples = SwapChain::GetNumSamples();
        unlitPipelineDesc.multisampling.sampleShadingEnable = VK_FALSE;
        unlitPipelineDesc.multisampling.minSampleShading = 0.5f;
        unlitPipelineDesc.multisampling.pSampleMask = nullptr;

        unlitPipelineDesc.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        unlitPipelineDesc.depthStencil.depthTestEnable = VK_TRUE;
        unlitPipelineDesc.depthStencil.depthWriteEnable = VK_TRUE;
        unlitPipelineDesc.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        unlitPipelineDesc.depthStencil.depthBoundsTestEnable = VK_FALSE;
        unlitPipelineDesc.depthStencil.minDepthBounds = 0.0f;
        unlitPipelineDesc.depthStencil.maxDepthBounds = 1.0f;
        unlitPipelineDesc.depthStencil.stencilTestEnable = VK_FALSE;
        unlitPipelineDesc.depthStencil.front = {};
        unlitPipelineDesc.depthStencil.back = {};

        unlitPipelineDesc.colorBlendAttachment.colorWriteMask =  VK_COLOR_COMPONENT_R_BIT;
        unlitPipelineDesc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        unlitPipelineDesc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        unlitPipelineDesc.colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        unlitPipelineDesc.colorBlendAttachment.blendEnable = VK_TRUE;
        unlitPipelineDesc.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        unlitPipelineDesc.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        unlitPipelineDesc.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        unlitPipelineDesc.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        unlitPipelineDesc.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        unlitPipelineDesc.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        unlitPipelineDesc.colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        unlitPipelineDesc.colorBlendState.logicOpEnable = VK_FALSE;
        unlitPipelineDesc.colorBlendState.logicOp = VK_LOGIC_OP_COPY;
        unlitPipelineDesc.colorBlendState.attachmentCount = 1;
        unlitPipelineDesc.colorBlendState.pAttachments = &unlitPipelineDesc.colorBlendAttachment;
        unlitPipelineDesc.colorBlendState.blendConstants[0] = 0.0f;
        unlitPipelineDesc.colorBlendState.blendConstants[1] = 0.0f;
        unlitPipelineDesc.colorBlendState.blendConstants[2] = 0.0f;
        unlitPipelineDesc.colorBlendState.blendConstants[3] = 0.0f;

        unlitPipelineDesc.bindings.resize(2);
        unlitPipelineDesc.bindings[0].binding = 0;
        unlitPipelineDesc.bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        unlitPipelineDesc.bindings[0].descriptorCount = 1;
        // here we specify in which shader stages the buffer will by referenced
        unlitPipelineDesc.bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // only relevant for image sampling related descriptors
        unlitPipelineDesc.bindings[0].pImmutableSamplers = nullptr;

        unlitPipelineDesc.bindings[1].binding = 1;
        unlitPipelineDesc.bindings[1].descriptorCount = 1;
        unlitPipelineDesc.bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        unlitPipelineDesc.bindings[1].pImmutableSamplers = nullptr;
        unlitPipelineDesc.bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    void createGraphicsPipeline() {
        unlitPipelineDesc.multisampling.rasterizationSamples = SwapChain::GetNumSamples();
        GraphicsPipeline::Create(unlitPipelineDesc, unlitPipeline);
    }

    void createCommandBuffers() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        commandBuffers.resize(SwapChain::GetNumFrames());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = LogicalDevice::GetCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSyncObjects() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(SwapChain::GetNumFrames(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
                || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS 
                || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void imguiDrawFrame() {
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
            GraphicsPipeline::OnImgui(unlitPipelineDesc, unlitPipeline);
            camera.OnImgui();
        }
        ImGui::End();

        Scene::OnImgui();

        ImGuizmo::BeginFrame();
        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
        static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

        if (ImGui::Begin("Transform")) {
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
                glm::value_ptr(transform.scale), glm::value_ptr(ubo.model));

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

            glm::mat4 guizmoProj(ubo.proj);
            guizmoProj[1][1] *= -1;

            ImGuiIO& io = ImGui::GetIO();
            ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
            ImGuizmo::Manipulate(glm::value_ptr(ubo.view), glm::value_ptr(guizmoProj), currentGizmoOperation,
                currentGizmoMode, glm::value_ptr(ubo.model), nullptr, nullptr);
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(ubo.model), glm::value_ptr(transform.position), 
                glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
        }
        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        imguiDrawData = ImGui::GetDrawData();
    }

    void updateCommandBuffer(size_t frameIndex) {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffers[frameIndex], &beginInfo) != VK_SUCCESS) {
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

        vkCmdBeginRenderPass(commandBuffers[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, unlitPipeline.pipeline);

        VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(commandBuffers[frameIndex], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[frameIndex], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        // command buffer, vertex count, instance count, first vertex, first instance
        // vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);

        vkCmdBindDescriptorSets(commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, unlitPipeline.layout, 0,
            1, &descriptorSets[frameIndex], 0, nullptr);
        vkCmdDrawIndexed(commandBuffers[frameIndex], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(imguiDrawData, commandBuffers[frameIndex]);

        vkCmdEndRenderPass(commandBuffers[frameIndex]);

        if (vkEndCommandBuffer(commandBuffers[frameIndex]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void drawFrame() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            device, SwapChain::GetVkSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(imageIndex);
        imguiDrawFrame();

        // check if a previous frame is using this image
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];
        updateCommandBuffer(imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        if (vkQueueSubmit(LogicalDevice::GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { SwapChain::GetVkSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        result = vkQueuePresentKHR(LogicalDevice::GetPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Window::GetFramebufferResized() || SwapChain::IsDirty() || unlitPipeline.dirty) {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recreateSwapChain() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        // busy wait while the window is minimized
        while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
            Window::WaitEvents();
        }
        Window::UpdateFramebufferSize();

        vkDeviceWaitIdle(device);

        cleanupSwapChain();

        PhysicalDevice::OnSurfaceUpdate();
        SwapChain::Create();
        MAX_FRAMES_IN_FLIGHT = SwapChain::GetFramesInFlight();
        createGraphicsPipeline();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
        initImgui();
        createUniformProjection();
    }

    void createUniformBuffers() {
        auto numFrames = SwapChain::GetNumFrames();
        uniformBuffers.resize(numFrames);

        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = sizeof(UniformBufferObject);

        for (size_t i = 0; i < numFrames; i++) {
            Buffers::Create(uniformDesc, uniformBuffers[i]);
        }
    }

    void createUniformModelView() {
        ubo.model = glm::mat4(1.0f);
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = SwapChain::GetExtent();
        camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        ubo.view = camera.GetView();
        ubo.proj = camera.GetProj();
        Buffers::Update(uniformBuffers[currentImage], &ubo, sizeof(ubo));
    }

    void createDescriptorPool() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        auto numFrames = SwapChain::GetNumFrames();
        uint32_t sets = static_cast<uint32_t>(numFrames);

        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(numFrames);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(numFrames + 1);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 2*static_cast<uint32_t>(numFrames);
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        auto numFrames = SwapChain::GetNumFrames();
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitPipeline.descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(numFrames);
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < numFrames; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrites[0].dstSet = descriptorSets[i];
descriptorWrites[0].dstBinding = 0;
// in the case of our descriptors being arrays, we specify the index
descriptorWrites[0].dstArrayElement = 0;
descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
descriptorWrites[0].descriptorCount = 1;
descriptorWrites[0].pBufferInfo = &bufferInfo;

descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrites[1].dstSet = descriptorSets[i];
descriptorWrites[1].dstBinding = 1;
descriptorWrites[1].dstArrayElement = 0;
descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
descriptorWrites[1].descriptorCount = 1;
descriptorWrites[1].pImageInfo = &imageInfo;

vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, 
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        // tiling cannot be changed later, if we want to acces the directly
        // access the texels of this image, we should use LINEAR
        imageInfo.tiling = tiling;
        // not usable by the GPU, the first transition will discard the texels
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // sampled defines that the image will be accessed by the shader
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = numSamples;
        imageInfo.flags = 0;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = PhysicalDevice::FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        return LogicalDevice::BeginSingleTimeCommands();
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        LogicalDevice::EndSingleTimeCommands(commandBuffer);
    }

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(PhysicalDevice::GetVkPhysicalDevice(), imageFormat, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            if (mipWidth > 1) {
                mipWidth /= 2;
            }
            if (mipHeight > 1) {
                mipHeight /= 2;
            }
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    void createTextureImage() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        BufferResource staging;
        Buffers::CreateStagingBuffer(staging, pixels, imageSize);

        stbi_image_free(pixels);
        
        createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, colorFormat, VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
        
        transitionImageLayout(textureImage, colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
        copyBufferToImage(staging.buffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
        generateMipmaps(textureImage, colorFormat, texWidth, texHeight, mipLevels);

        Buffers::Destroy(staging);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        // if we were transferring queue family ownership
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
    
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
    
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        endSingleTimeCommands(commandBuffer);
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        // what the purpose of the image and how it will be accessed
        // our images will be used as color targets without any mipmapping
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void createTextureImageView() {
        auto image = VulkanImage();
        image.Create(textureImage, mipLevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        textureImageView = image.view;
    }

    void createTextureSampler() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;

        // get the max ansotropy level of my device
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice::GetVkPhysicalDevice(), &properties);

        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        // what color to return when clamp is active in addressing mode
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        // if comparison is enabled, texels will be compared to a value an the result 
        // is used in filtering operations, can be used in PCF on shadow maps
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
    
    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void loadModel() {
        if (vertices.size() || indices.size()) {
            return;
        }
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
            throw std::runtime_error(warn + err);
        }

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void firstInitImgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::StyleColorsDark();
    }

    void initImgui() {
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
        initInfo.DescriptorPool = descriptorPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = (uint32_t)SwapChain::GetNumFrames();
        initInfo.MSAASamples = SwapChain::GetNumSamples();
        initInfo.Allocator = VK_NULL_HANDLE;
        initInfo.CheckVkResultFn = CheckVulkanResult;
        ImGui_ImplVulkan_Init(&initInfo, SwapChain::GetRenderPass());
        
        auto commandBuffer = beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        endSingleTimeCommands(commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void cleanupImgui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }
    
    void finishImgui() {
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
