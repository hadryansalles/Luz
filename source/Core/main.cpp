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
        Setup();
        Create();
        MainLoop();
        Destroy();
    }

private:
    const std::string MODEL_PATH = "assets/models/converse.obj";
    const std::string TEXTURE_PATH = "assets/models/checker.png";

    SceneUBO sceneUBO;

    // image 
    uint32_t mipLevels;
    VkImage textureImage;
    VkSampler textureSampler;
    VkImageView textureImageView;
    VkDeviceMemory textureImageMemory;
    VkDescriptorSet textureDescriptor;

    TextureResource* textureResource;

    // imgui
    ImDrawData* imguiDrawData = nullptr;

    // camera 
    Camera camera;

    void Setup() {
        UnlitGraphicsPipeline::Setup();
        SceneManager::Setup();
    }

    void Create() {
        SetupImgui();
        Window::Create();
        Instance::Create();
        PhysicalDevice::Create();
        LogicalDevice::Create();
        SwapChain::Create();
        DEBUG_TRACE("Finish creating SwapChain.");
        GraphicsPipelineManager::Create();
        UnlitGraphicsPipeline::Create();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createTextureDescriptor();
        DEBUG_TRACE("Finish loading model.");
        CreateImgui();
        createUniformProjection();
        DEBUG_TRACE("Finish initializing Vulkan.");
        SceneManager::Create();
    }
    
    void createTextureDescriptor() {
        auto device = LogicalDevice::GetVkDevice();
        auto allocator = Instance::GetAllocator();
        auto unlitGPO = UnlitGraphicsPipeline::GetResource();

        textureResource = AssetManager::LoadImageFile(TEXTURE_PATH);

        std::vector<VkDescriptorSetLayout> layouts(1, unlitGPO.textureDescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = GraphicsPipelineManager::GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts.data();

        auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, &textureDescriptor);

        DEBUG_VK(vkRes, "Failed to allocate scene descriptor sets!");
        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureResource->image.view;
        imageInfo.sampler = textureResource->sampler;

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = textureDescriptor;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void Destroy() {
        DestroyVulkan();
        FinishImgui();
    }

    void DestroyVulkan() {
        SceneManager::Destroy();
        MeshManager::Destroy();
        TextureManager::Destroy();

        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();

        DestroySwapChain();

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        LogicalDevice::Destroy();
        PhysicalDevice::Destroy();
        Instance::Destroy();
        Window::Destroy();
    }

    void DestroySwapChain() {
        UnlitGraphicsPipeline::Destroy();
        GraphicsPipelineManager::Destroy();

        DestroyImgui();

        SwapChain::Destroy();
        LOG_INFO("Destroyed SwapChain.");
    }

    void MainLoop() {
        auto instance = Instance::GetVkInstance();
        while (!Window::GetShouldClose()) {
            Window::Update();
            camera.Update();
            drawFrame();
            if (Instance::IsDirty() || PhysicalDevice::IsDirty() || Window::IsDirty()) {
                vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
                Destroy();
                Create();
            }
        }
        vkDeviceWaitIdle(LogicalDevice::GetVkDevice());
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
            UnlitGraphicsPipeline::OnImgui();
            camera.OnImgui();
        }
        ImGui::End();

        SceneManager::OnImgui();

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

        auto sceneDescriptor = SceneManager::GetSceneDescriptor(frameIndex);
        auto unlitGPO = UnlitGraphicsPipeline::GetResource();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.layout, 0,
                    1, &sceneDescriptor, 0, nullptr);

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
                    1, &model->descriptors[frameIndex], 0, nullptr);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlitGPO.layout, 2,
                    1, &model->materialDescriptors[frameIndex], 0, nullptr);
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
        imguiDrawFrame();

        auto image = SwapChain::Acquire(); 

        updateUniformBuffer(image);
        updateCommandBuffer(image);

        SwapChain::SubmitAndPresent(image);
        if (dirtyFrameResources()) {
            recreateFrameResources();
            return;
        }
    }

    bool dirtyFrameResources() {
        return SwapChain::IsDirty() || Window::GetFramebufferResized() || UnlitGraphicsPipeline::IsDirty();
    }

    void recreateFrameResources() {
        auto device = LogicalDevice::GetVkDevice();
        auto instance = Instance::GetVkInstance();
        vkDeviceWaitIdle(device);
        if (Window::GetFramebufferResized() || SwapChain::IsDirty()) {
            // busy wait while the window is minimized
            while (Window::GetWidth() == 0 || Window::GetHeight() == 0) {
                Window::WaitEvents();
            }
            Window::UpdateFramebufferSize();
            vkDeviceWaitIdle(device);
            DestroySwapChain();
            PhysicalDevice::OnSurfaceUpdate();
            SwapChain::Create();
            GraphicsPipelineManager::Create();
            UnlitGraphicsPipeline::Create();
            SceneManager::RecreateDescriptors();
            createTextureDescriptor();
            CreateImgui();
            createUniformProjection();
        } else if (UnlitGraphicsPipeline::IsDirty()) {
            UnlitGraphicsPipeline::Destroy();
            UnlitGraphicsPipeline::Create();
        }
    }

    void createUniformProjection() {
        // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        // the easiest way to fix this is fliping the scaling factor of the y axis
        auto ext = SwapChain::GetExtent();
        camera.SetExtent(ext.width, ext.height);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        for (Model* model : SceneManager::GetModels()) {
            BufferManager::Update(model->buffers[currentImage], &model->ubo, sizeof(model->ubo));
        }

        sceneUBO.view = camera.GetView();
        sceneUBO.proj = camera.GetProj();
        BufferManager::Update(SceneManager::GetUniformBuffer(currentImage), &sceneUBO, sizeof(sceneUBO));
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
        BufferManager::CreateStagingBuffer(staging, pixels, imageSize);

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

        BufferManager::Destroy(staging);
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
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
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

    void SetupImgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::StyleColorsDark();
    }

    void CreateImgui() {
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
        initInfo.DescriptorPool = GraphicsPipelineManager::GetDescriptorPool();
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
