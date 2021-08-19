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

    newModels = AssetManager::LoadObjFile("assets/models/ignore/coffee_cart/coffee_cart.obj");
    for (Model* model : newModels) {
        SceneManager::AddModel(model);
    }

    newModels = AssetManager::LoadObjFile("assets/models/ignore/sponza/sponza_mini.obj");
    for (Model* model : newModels) {
        SceneManager::AddModel(model);
    }
}

void CreateModelDescriptors(Model* model) {
    model->meshDescriptor = GraphicsPipelineManager::CreateMeshDescriptor(sizeof(ModelUBO));
    model->materialDescriptor = GraphicsPipelineManager::CreateMaterialDescriptor();
    GraphicsPipelineManager::UpdateBufferDescriptor(model->meshDescriptor, &model->ubo, sizeof(model->ubo));
}

void SceneManager::Create() {
    sceneDescriptor = GraphicsPipelineManager::CreateSceneDescriptor(sizeof(SceneUBO));
    GraphicsPipelineManager::UpdateBufferDescriptor(sceneDescriptor, &sceneUBO, sizeof(sceneUBO));

    for (Model* model : models) {
        CreateModelDescriptors(model);
        if (model->texture) {
            SceneManager::SetTexture(model, model->texture);
        }
    }
}

void SceneManager::Destroy() {
    for (size_t i = 0; i < sceneDescriptor.buffers.size(); i++) {
        BufferManager::Destroy(sceneDescriptor.buffers[i]);
    }

    for (Model* model : models) {
        for (BufferResource& buffer : model->meshDescriptor.buffers) {
            BufferManager::Destroy(buffer);
        }
    }
}

void SceneManager::Finish() {
    for (Model* model : models) {
        delete model;
    }
}

void SceneManager::SetTexture(Model* model, TextureResource* texture) {
    model->texture = texture;
    GraphicsPipelineManager::UpdateTextureDescriptor(model->materialDescriptor, texture);
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
