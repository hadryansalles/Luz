#include "Luzpch.hpp"

#include "Light.hpp"
#include "SwapChain.hpp"
#include "GraphicsPipelineManager.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"

void LightManager::Create() {
    auto numFrames = SwapChain::GetNumFrames();
    auto bindlessDescriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    std::vector<VkDescriptorBufferInfo> bufferInfos(numFrames);
    std::vector<VkWriteDescriptorSet> writes(numFrames);

    // create a buffer with one section for each frame
    BufferDesc bufferDesc;
    VkDeviceSize minOffset = PhysicalDevice::GetProperties().limits.minUniformBufferOffsetAlignment;
    VkDeviceSize uniformSize = sizeof(uniformData);
    VkDeviceSize sizeRemain = uniformSize % minOffset;
    sectionSize = uniformSize;
    if (sizeRemain > 0) {
        sectionSize += minOffset - sizeRemain;
    }
    bufferDesc.size = sectionSize * numFrames;
    bufferDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    BufferManager::Create(bufferDesc, buffer);

    SetDirty();
    for (int i = 0; i < numFrames; i++) {
        UpdateBufferIfDirty(i);

        bufferInfos[i].buffer = buffer.buffer;
        bufferInfos[i].offset = i*sectionSize;
        bufferInfos[i].range = uniformSize;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = GraphicsPipelineManager::BUFFERS_BINDING;
        writes[i].dstArrayElement = numFrames*GraphicsPipelineManager::LIGHTS_BUFFER_INDEX + i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }
    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), numFrames, writes.data(), 0, nullptr);
}

void LightManager::Destroy() {
    BufferManager::Destroy(buffer);
}

void LightManager::Finish() {
    for (Light* light : lights) {
        delete light;
    }
    lights.clear();
}

void LightManager::PointLightOnImgui(Light* light) {

}

void LightManager::OnImgui() {
    if (ImGui::CollapsingHeader("LightManager")) {
        if (ImGui::TreeNodeEx("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorEdit3("Color", glm::value_ptr(uniformData.ambientColor))) {
                SetDirty();
            }
            if (ImGui::DragFloat("Intensity", &uniformData.ambientIntensity, 0.001f, 0.0f)) {
                SetDirty();
            }
            ImGui::TreePop();
        }
    }
}

void LightManager::OnImgui(Light* light) {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("typeCombo");
        if (ImGui::BeginCombo("", LightManager::TYPE_NAMES[light->type])) {
            if (ImGui::Selectable("Point", light->type == LightType::Point)) {
                light->type = LightType::Point;
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        ImGui::Text("Color");
        ImGui::SameLine();
        ImGui::PushID("color");
        ImGui::ColorEdit4("", glm::value_ptr(light->color));
        ImGui::PopID();
        ImGui::Text("Intensity");
        ImGui::SameLine();
        ImGui::PushID("intensity");
        ImGui::DragFloat("", &light->intensity, 0.01f, 0.0f);
        ImGui::PopID();
    }
}

void LightManager::SetDirty() {
    dirtyBuffer = std::vector<bool>(SwapChain::GetNumFrames(), true);
    dirtyUniform = true;
}

Light* LightManager::CreateLight(LightType type) {
    Light* light = new Light;
    lights.push_back(light);
    SetDirty();
    return light;
}

void LightManager::DestroyLight(Light* light) {
    lights.erase(std::find(lights.begin(), lights.end(), light));
    SetDirty();
}

void LightManager::SetAmbient(glm::vec3 color, float intensity) {
    uniformData.ambientColor = color;
    uniformData.ambientIntensity = intensity;
    SetDirty();
}

void LightManager::UpdateUniformIfDirty() {
    if (dirtyUniform) {
        uniformData.numPointLights = 0;
        for (Light* light : lights) {
            switch (light->type) {
            case LightType::Point:
                PointLight& pointLight = uniformData.pointLights[uniformData.numPointLights];
                pointLight.color = light->color;
                pointLight.intensity = light->intensity;
                pointLight.position = light->transform.position;
                uniformData.numPointLights += 1;
                break;
            }
        }
        dirtyUniform = false;
    }
}

void LightManager::UpdateBufferIfDirty(int numFrame) {
    DEBUG_ASSERT(numFrame < dirtyBuffer.size(), "Invalid frame number!");
    UpdateUniformIfDirty();
    if (dirtyBuffer[numFrame]) {
        BufferManager::Update(buffer, &uniformData, numFrame * sectionSize, sizeof(uniformData));
        dirtyBuffer[numFrame] = false;
    }
}
