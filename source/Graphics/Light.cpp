#include "Luzpch.hpp"

#include "Light.hpp"
#include "SwapChain.hpp"
#include "GraphicsPipelineManager.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "SceneManager.hpp"
#include "AssetManager.hpp"
#include "UnlitGraphicsPipeline.hpp"

void LightManager::Create() {
    if (lightMeshes[0] == nullptr) {
        auto pointDesc = AssetManager::LoadMeshFile("assets/point.obj");
        DEBUG_ASSERT(pointDesc.size() > 0, "Can't load point light gizmo mesh!");
        auto dirDesc = AssetManager::LoadMeshFile("assets/directional.obj");
        DEBUG_ASSERT(pointDesc.size() > 0, "Can't load directional light gizmo mesh!");
        auto spotDesc = AssetManager::LoadMeshFile("assets/spot.obj");
        DEBUG_ASSERT(pointDesc.size() > 0, "Can't load spot light gizmo mesh!");
        lightMeshes[0] = MeshManager::CreateMesh(pointDesc[0].mesh);
        lightMeshes[1] = MeshManager::CreateMesh(dirDesc[0].mesh);
        lightMeshes[2] = MeshManager::CreateMesh(spotDesc[0].mesh);
    }

    for (Light* light : lights) {
        light->model = SceneManager::CreateGizmoModel(lightMeshes[light->type]);
    }

    auto numFrames = SwapChain::GetNumFrames();
    auto bindlessDescriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    std::vector<VkDescriptorBufferInfo> bufferInfos(numFrames);
    std::vector<VkWriteDescriptorSet> writes(numFrames);

    // create a buffer with one section for each frame
    BufferManager::CreateUniformBuffer(uniformBuffer, sizeof(uniformData));

    SetDirty();
    GraphicsPipelineManager::WriteUniform(uniformBuffer, GraphicsPipelineManager::LIGHTS_BUFFER_INDEX);
}

void LightManager::Destroy() {
    BufferManager::DestroyUniformBuffer(uniformBuffer);
    for (Light* light : lights) {
        SceneManager::DestroyModel(light->model);
    }
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
        ImGui::Checkbox("Render gizmos", &renderGizmos);
        ImGui::DragFloat("Gizmos opacity", &gizmoOpacity, 0.01f, .0f, 1.0f);
        if (ImGui::TreeNodeEx("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorEdit3("Color", glm::value_ptr(uniformData.ambientColor))) {
                SetDirty();
            }
            if (ImGui::DragFloat("Intensity", &uniformData.ambientIntensity, 0.01f, 0.0f)) {
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
            if (ImGui::Selectable("Directional", light->type == LightType::Directional)) {
                light->type = LightType::Directional;
            }
            if (ImGui::Selectable("Spot", light->type == LightType::Spot)) {
                light->type = LightType::Spot;
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
        if (light->type == LightType::Spot) {
            ImGui::Text("Inner angle");
            ImGui::SameLine();
            ImGui::PushID("cutOff");
            ImGui::DragFloat("", &light->cutOff, 0.1f, 0.0f, 360.0f);
            ImGui::PopID();
            ImGui::Text("Outer angle");
            ImGui::SameLine();
            ImGui::PushID("outerCutOff");
            ImGui::DragFloat("", &light->outerCutOff, 0.1f, 0.0f, 360.0f);
            ImGui::PopID();
        }
    }
}

void LightManager::SetDirty() {
    BufferManager::SetDirtyUniform(uniformBuffer);
    dirtyUniform = true;
}

Light* LightManager::CreateLight(LightType type) {
    Light* light = new Light;
    light->id = lights.size();
    light->model = SceneManager::CreateGizmoModel(lightMeshes[light->type]);
    lights.push_back(light);
    SetDirty();
    return light;
}

Light* LightManager::CreateLightCopy(Light* copy) {
    Light* light = CreateLight(copy->type);
    light->color = copy->color;
    light->transform = copy->transform;
    light->intensity = copy->intensity;
    light->name = copy->name;
    return light;
}

void LightManager::DestroyLight(Light* light) {
    lights.erase(std::find(lights.begin(), lights.end(), light));
    SceneManager::DestroyModel(light->model);
    delete light;
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
        uniformData.numDirectionalLights = 0;
        uniformData.numSpotLights = 0;
        for (Light* light : lights) {
            light->model->mesh = lightMeshes[light->type];
            light->model->transform.SetMatrix(light->transform.GetMatrix());
            light->model->material.diffuseColor = light->color;
            light->model->material.diffuseColor.a = gizmoOpacity;
            if (light->type == LightType::Point) {
                PointLight& pointLight = uniformData.pointLights[uniformData.numPointLights];
                pointLight.color = light->color;
                pointLight.intensity = light->intensity;
                pointLight.position = glm::vec3(light->transform.parent->GetMatrix() * glm::vec4(light->transform.position, 1));
                uniformData.numPointLights += 1;
                light->model->transform.SetRotation(glm::vec3(.0f, .0f, .0f));
            } else if (light->type == LightType::Directional) {
                DirectionalLight& dirLight = uniformData.directionalLights[uniformData.numDirectionalLights];
                dirLight.color = light->color;
                dirLight.intensity = light->intensity;
                dirLight.direction = glm::normalize(light->transform.GetGlobalFront());
                uniformData.numDirectionalLights += 1;
            } else if (light->type == LightType::Spot) {
                SpotLight& spotLight = uniformData.spotLights[uniformData.numSpotLights];
                spotLight.color = light->color;
                spotLight.intensity = light->intensity;
                spotLight.direction = glm::normalize(light->transform.GetGlobalFront());
                spotLight.position = glm::vec3(light->transform.parent->GetMatrix() * glm::vec4(light->transform.position, 1));
                spotLight.outerCutOff = glm::radians(light->outerCutOff);
                spotLight.cutOff = glm::radians(light->cutOff);
                uniformData.numSpotLights += 1;
            }
        }
        dirtyUniform = false;
    }
}

void LightManager::UpdateBufferIfDirty(int numFrame) {
    // DEBUG_ASSERT(numFrame < uniformBuffer.dirty.size(), "Invalid frame number!");
    // UpdateUniformIfDirty();
    // if (uniformBuffer.dirty[numFrame]) {
    //     BufferManager::UpdateUniformIfDirty(uniformBuffer, numFrame, &uniformData);
    //     for (Light* light : lights) {
    //         Model* model = light->model;
    //         model->ubo.model = model->transform.GetMatrix();
    //         BufferManager::Update(model->meshDescriptor.buffers[numFrame], &model->ubo, sizeof(model->ubo));
    //         BufferManager::Update(model->material.materialDescriptor.buffers[numFrame], (UnlitMaterialUBO*)&(model->material), sizeof(UnlitMaterialUBO));
    //     }
    // }
}
