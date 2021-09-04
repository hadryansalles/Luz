#include "Luzpch.hpp"

#include "SceneManager.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "SwapChain.hpp"
#include "UnlitGraphicsPipeline.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include "Camera.hpp"

#include <future>

void SceneManager::Setup() {
    mainCollection.name = "Root";

    std::vector<ModelDesc> newModels;

    // auto cube = AssetManager::LoadMeshFile("assets/models/cube.obj")[0];
    // cube.texture = AssetManager::LoadImageFile("assets/models/cube.png");
    // AddModel(CreateModel(cube));

    AsyncLoadModels("assets/models/teapot.obj");
    // AsyncLoadModels("assets/models/ignore/sponza/sponza_semitransparent.obj");
}

void SceneManager::LoadModels(std::filesystem::path path, Collection* collection) {
    AssetManager::AddObjFileToScene(path, collection);
}

void SceneManager::AsyncLoadModels(std::filesystem::path path, Collection* collection) {
    std::thread(LoadModels, path, collection).detach();
}

void CreateModelDescriptors(Model* model) {
    model->meshDescriptor = GraphicsPipelineManager::CreateMeshDescriptor(sizeof(ModelUBO));
    model->materialDescriptor = GraphicsPipelineManager::CreateMaterialDescriptor();
    GraphicsPipelineManager::UpdateBufferDescriptor(model->meshDescriptor, &model->ubo, sizeof(model->ubo));
}

void SceneManager::AddPreloadedModel(ModelDesc desc) {
    preloadedModelsLock.lock();
    preloadedModels.push_back(desc);
    preloadedModelsLock.unlock();
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

    for (Collection* collection : collections) {
        delete collection;
    }
}

void SceneManager::Update() {
    LUZ_PROFILE_FUNC();
    preloadedModelsLock.lock();
    if (preloadedModels.size() > 0) {
        for (int i = 0; i < preloadedModels.size(); i++) {
            Model* newModel = CreateModel(preloadedModels[i]);
            AddModel(newModel, preloadedModels[i].collection);
        }
        preloadedModels.clear();
    }
    preloadedModelsLock.unlock();
}

void SceneManager::AddModel(Model* model, Collection* collection) {
    models.push_back(model);
    SetCollection(model, collection);
}

void SceneManager::SetCollection(Model* model, Collection* collection) {
    if (model->collection != nullptr) {
        auto it = std::find(model->collection->models.begin(), model->collection->models.end(), model);
        if (it != model->collection->models.end()) {
            model->collection->models.erase(it);
        }
    }
    if (collection == nullptr) {
        collection = &mainCollection;
    }
    collection->models.push_back(model);
    model->transform.parent = &(collection->transform);
    model->collection = collection;
}

void SceneManager::SetTexture(Model* model, TextureResource* texture) {
    model->texture = texture;
    GraphicsPipelineManager::UpdateTextureDescriptor(model->materialDescriptor, texture);
}

Model* SceneManager::CreateModel(ModelDesc& desc) {
    Model* model = new Model();
    model->mesh = MeshManager::CreateMesh(desc.mesh);
    model->name = desc.mesh->name;
    model->id = modelID++;
    model->transform.position += model->mesh->center;
    CreateModelDescriptors(model);
    if (desc.texture.data != nullptr) {
        SetTexture(model, TextureManager::CreateTexture(desc.texture));
    }
    else {
        SetTexture(model, TextureManager::GetDefaultTexture());
    }
    return model;
}

void DirOnImgui(std::filesystem::path path) {
    if (ImGui::TreeNode(path.filename().string().c_str())) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                DirOnImgui(entry.path());
            }
            else {
                std::string filePath = entry.path().string();
                std::string fileName = entry.path().filename().string();
                ImGui::PushID(filePath.c_str());
                if (ImGui::Selectable(fileName.c_str())) {
                }
                if (AssetManager::IsMeshFile(entry.path())) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("mesh", filePath.c_str(), filePath.size());
                        ImGui::Text(fileName.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
}

void SceneManager::ModelOnImgui(Model* model) {
    bool selected = model == selectedModel;
    ImGui::PushID(model->id);

    selected = ImGui::Selectable(model->name.c_str(), &selected);

    if(selected) {
        selectedModel = model;
        selectedTransform = &model->transform;
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("model", &model, sizeof(Model*));
        ImGui::Text(model->name.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::PopID();
}

void SceneManager::CollectionOnImgui(Collection* collection, int id) {
    ImGui::PushID(id);

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
    nodeFlags |= selectedTransform == &collection->transform ? ImGuiTreeNodeFlags_Selected : 0;

    bool open = ImGui::TreeNodeEx(collection->name.c_str(), nodeFlags);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedTransform = &collection->transform;
        selectedModel = nullptr;
    }

    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* meshPayload = ImGui::AcceptDragDropPayload("mesh");
        if (meshPayload) {
            std::string meshPath((const char*) meshPayload->Data, meshPayload->DataSize);
            AsyncLoadModels(meshPath, collection);
            ImGui::EndDragDropTarget();
        }

        const ImGuiPayload* modelPayload = ImGui::AcceptDragDropPayload("model");
        if (modelPayload) {
            Model* model = *(Model**) modelPayload->Data;
            SetCollection(model, collection);
            ImGui::EndDragDropTarget();
        }
    }

    if(open) {
        for (int i = 0; i < collection->children.size(); i++) {
            CollectionOnImgui(collection->children[i], i);
        }
        for (int i = 0; i < collection->models.size(); i++) {
            ModelOnImgui(collection->models[i]);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneManager::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    ImGui::Text("Path");
    ImGui::SameLine(totalWidth*3.0f/5.0);
    ImGui::Text(path.string().c_str());
    if (ImGui::CollapsingHeader("Collections")) {
        CollectionOnImgui(&mainCollection, -1);
    }
    if (ImGui::CollapsingHeader("Hierarchy")) {
        for (auto& model : models) {
            bool selected = model == selectedModel;
            ImGui::PushID(model->id);
            if (ImGui::Selectable(model->name.c_str(), &selected)) {
                selectedModel = model;
                selectedTransform = &model->transform;
            }
            ImGui::PopID();
        }
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("mesh");
            if (payload) {
                std::string meshPath((const char*) payload->Data, payload->DataSize);
                AsyncLoadModels(meshPath);
                ImGui::EndDragDropTarget();
            }
        }
    }
    if (ImGui::CollapsingHeader("Files")) {
        ImGui::Text("Auto Reload");
        ImGui::SameLine(totalWidth*3.0f/5.0f);
        ImGui::PushID("autoReload");
        ImGui::Checkbox("", &autoReloadFiles);
        ImGui::PopID();
        DirOnImgui(path.parent_path());
    }
}

Collection* SceneManager::CreateCollection(Collection* parent) {
    if (parent == nullptr) {
        parent = &mainCollection;
    }
    Collection* collection = new Collection();
    collection->parent = parent;
    collection->transform.parent = &parent->transform;
    parent->children.push_back(collection);
    collections.push_back(collection);
    return collection;
}
