#include "Luzpch.hpp"

#include "SceneManager.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "SwapChain.hpp"
#include "UnlitGraphicsPipeline.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include "Camera.hpp"

void SceneManager::Setup() {
    std::vector<Model*> newModels;

    newModels = AssetManager::LoadObjFile("assets/models/cube.obj");
    // SceneManager::SetTexture(newModels[0], AssetManager::LoadImageFile("assets/models/cube.png"));
    SceneManager::AddModel(newModels[0]);

    // newModels = AssetManager::LoadObjFile("assets/models/converse.obj");
    // SceneManager::SetTexture(newModels[0], AssetManager::LoadImageFile("assets/models/converse.jpg"));
    // SceneManager::AddModel(newModels[0]);

    newModels = AssetManager::LoadObjFile("assets/models/teapot.obj");
    for (Model* model : newModels) {
        SceneManager::AddModel(model);
    }

    newModels = AssetManager::LoadObjFile("assets/models/ignore/sponza_mini.obj");
    for (Model* model : newModels) {
        SceneManager::AddModel(model);
    }
}

void CreateModelDescriptors(Model* model) {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();
    auto unlitGPO = UnlitGraphicsPipeline::GetResource();

    std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitGPO.modelDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = GraphicsPipelineManager::GetDescriptorPool();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = layouts.data();

    model->descriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, model->descriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate transform descriptor sets!");

    std::vector<VkDescriptorSetLayout> matLayouts(numFrames, unlitGPO.textureDescriptorSetLayout);

    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = GraphicsPipelineManager::GetDescriptorPool();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = matLayouts.data();

    model->materialDescriptors.resize(numFrames);
    vkRes = vkAllocateDescriptorSets(device, &allocInfo, model->materialDescriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate texture descriptor sets!");

    for (size_t i = 0; i < numFrames; i++) {
        model->buffers.resize(numFrames);

        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = sizeof(ModelUBO);

        BufferManager::Create(uniformDesc, model->buffers[i]);
    }

    for (size_t i = 0; i < numFrames; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = model->buffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ModelUBO);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = model->descriptors[i];
        descriptorWrites[0].dstBinding = 0;
        // in the case of our descriptors being arrays, we specify the index
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void SceneManager::Create() {
    auto numFrames = SwapChain::GetNumFrames();
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();
    auto unlitGPO = UnlitGraphicsPipeline::GetResource();

    std::vector<VkDescriptorSetLayout> layouts(numFrames, unlitGPO.sceneDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = GraphicsPipelineManager::GetDescriptorPool();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
    allocInfo.pSetLayouts = layouts.data();

    sceneDescriptors.resize(numFrames);
    auto vkRes = vkAllocateDescriptorSets(device, &allocInfo, sceneDescriptors.data());
    DEBUG_VK(vkRes, "Failed to allocate scene descriptor sets!");

    for (size_t i = 0; i < numFrames; i++) {
        sceneBuffers.resize(numFrames);

        BufferDesc uniformDesc;
        uniformDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniformDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniformDesc.size = sizeof(SceneUBO);

        BufferManager::Create(uniformDesc, sceneBuffers[i]);
    }

    for (size_t i = 0; i < numFrames; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = sceneBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneUBO);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = sceneDescriptors[i];
        descriptorWrites[0].dstBinding = 0;
        // in the case of our descriptors being arrays, we specify the index
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    for (Model* model : models) {
        CreateModelDescriptors(model);
        if (model->texture) {
            SceneManager::SetTexture(model, model->texture);
        }
    }
}

void SceneManager::Destroy() {
    for (size_t i = 0; i < sceneBuffers.size(); i++) {
        BufferManager::Destroy(sceneBuffers[i]);
    }
    sceneBuffers.clear();
    sceneDescriptors.clear();

    for (Model* model : models) {
        for (BufferResource& buffer : model->buffers) {
            BufferManager::Destroy(buffer);
        }
        model->buffers.clear();
        model->descriptors.clear();
    }
}

void SceneManager::Finish() {
    for (Model* model : models) {
        delete model;
    }
}

void SceneManager::SetTexture(Model* model, TextureResource* texture) {
    std::array<VkWriteDescriptorSet, 1> writes{};

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture->image.view;
    imageInfo.sampler = texture->sampler;

    for (size_t i = 0; i < SwapChain::GetNumFrames(); i++) {
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = model->materialDescriptors[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), (uint32_t)(writes.size()), writes.data(), 0, nullptr);
    }
    
    model->texture = texture;
}

Model* SceneManager::CreateModel() {
    Model* model = new Model();
    model->name = "Default";
    CreateModelDescriptors(model);
    return model;
}

void DirOnImgui(std::filesystem::path path) {
    if (ImGui::TreeNode(path.filename().string().c_str())) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                DirOnImgui(entry.path());
            }
            else {
                ImGui::Text(entry.path().filename().string().c_str());
            }
        }
        ImGui::TreePop();
    }
}

void SceneManager::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    if (ImGui::Begin("Scene")) {
        if (ImGui::CollapsingHeader("Hierarchy")) {
            for (auto& model : models) {
                bool selected = model == selectedModel;
                if (ImGui::Selectable(model->name.c_str(), &selected)) {
                    selectedModel = model;
                }
            }
        }
        ImGui::Text("Path");
        ImGui::SameLine(totalWidth*3.0f/5.0);
        ImGui::Text(path.string().c_str());
        if (ImGui::CollapsingHeader("Files")) {
            ImGui::Text("Auto Reload");
            ImGui::SameLine(totalWidth*3.0f/5.0f);
            ImGui::PushID("autoReload");
            ImGui::Checkbox("", &autoReloadFiles);
            ImGui::PopID();
            for (const auto& entry : std::filesystem::directory_iterator(path.parent_path())) {
                if (entry.is_directory()) {
                    DirOnImgui(entry.path());
                }
                else {
                    ImGui::Text(entry.path().filename().string().c_str());
                }
            }
        }
    }
    ImGui::End();
}
