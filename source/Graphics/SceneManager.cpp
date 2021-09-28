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

    // auto cube = AssetManager::LoadMeshFile("assets/cube.obj")[0];
    // cube.texture = AssetManager::LoadImageFile("assets/cube.png");
    // AddModel(CreateModel(cube));

    auto planeCollection = CreateCollection();
    planeCollection->name = "Plane";

    AsyncLoadModels("assets/teapot.obj");
    AsyncLoadModels("assets/cube.obj");
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
        DeleteModelFromCollection(model);
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
                if (AssetManager::IsTextureFile(entry.path())) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("texture", filePath.c_str(), filePath.size());
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

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        SelectModel(model);
        openSceneItemMenu = true;
    }

    if(selected) {
        SelectModel(model);
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
        SelectCollection(collection);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        SelectCollection(collection);
        openSceneItemMenu = true;
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("collection", &collection, sizeof(Collection*));
        ImGui::Text(collection->name.c_str());
        ImGui::EndDragDropSource();
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

        const ImGuiPayload* collectionPayload = ImGui::AcceptDragDropPayload("collection");
        if (collectionPayload) {
            Collection* childCollection = *(Collection**) collectionPayload->Data;
            SetCollectionParent(childCollection, collection);
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
        openSceneItemMenu &= !ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        CollectionOnImgui(&mainCollection, -1);
        if (openSceneItemMenu) {
            ImGui::OpenPopup("right_click_hierarchy");
        }
        if (ImGui::BeginPopup("right_click_hierarchy")) {
            if (selectedModel != nullptr) {
                if (ImGui::MenuItem("Delete")) {
                    DeleteModel(selectedModel);
                    SelectModel(nullptr);
                }
            }
            if (selectedCollection != nullptr) {
                if (ImGui::MenuItem("Delete")) {
                    DeleteCollection(selectedCollection);
                    SelectCollection(nullptr);
                }
            }
            ImGui::EndPopup();
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
    collection->name = "New Collection";
    collection->parent = parent;
    collection->transform.parent = &parent->transform;
    parent->children.push_back(collection);
    collections.push_back(collection);
    return collection;
}

void SceneManager::SetCollectionParent(Collection* child, Collection* parent) {
    if (!CollectionCanBeChild(child, parent)) {
        LOG_WARN("Trying to set invalid collection child-parent.");
        return;
    }
    RemoveCollectionFromParent(child);
    child->parent = parent;
    child->transform.parent = &parent->transform;
    parent->children.push_back(child);
}

void SceneManager::RemoveCollectionFromParent(Collection* collection) {
    Collection* oldParent = collection->parent;
    if (oldParent != nullptr) {
        auto it = std::find(oldParent->children.begin(), oldParent->children.end(), collection);
        if (it == oldParent->children.end()) {
            LOG_ERROR("Trying to remove invalid collection from parent...");
            return;
        }
        oldParent->children.erase(it);
    }
    collection->parent = nullptr;
    collection->transform.parent = nullptr;
}

bool SceneManager::CollectionCanBeChild(Collection* child, Collection* parent) {
    bool cant = child == parent;
    while (parent != nullptr) {
        cant |= parent->parent == child;
        parent = parent->parent;
    }
    return !cant;
}

void SceneManager::DeleteCollection(Collection* collection) {
    auto it = std::find(collections.begin(), collections.end(), collection);
    if (it == collections.end()) {
        LOG_ERROR("Truing to delete invalid collection from scene...");
    }
    collections.erase(it);
    RemoveCollectionFromParent(collection);
    for (int i = 0; i < collection->children.size(); i++) {
        DeleteCollection(collection->children[i]);
    }
    for (int i = 0; i < collection->models.size(); i++) {
        DeleteModel(collection->models[i]);
    }
    delete collection;
}

void SceneManager::DeleteModelFromCollection(Model* model) {
    auto it = std::find(model->collection->models.begin(), model->collection->models.end(), model);
    if (it == model->collection->models.end()) {
        LOG_ERROR("Trying to delete invalid model from collection...");
        return;
    }
    model->collection->models.erase(it);
}

void SceneManager::DeleteModel(Model* model) {
    DeleteModelFromCollection(model);

    auto it = std::find(models.begin(), models.end(), model);
    if (it == models.end()) {
        LOG_ERROR("Trying to delete invalid model from scene...");
        return;
    }
    models.erase(it);

    delete model;
}

void SceneManager::SelectCollection(Collection* collection) {
    selectedModel = nullptr;
    selectedCollection = collection;
    selectedTransform = collection != nullptr ? &collection->transform : nullptr;
}

void SceneManager::SelectModel(Model* model) {
    selectedCollection = nullptr;
    selectedModel = model;
    selectedTransform = model != nullptr ? &model->transform : nullptr;
}

void SceneManager::LoadAndSetTexture(Model* model, std::filesystem::path path) {
    TextureResource* texture = TextureManager::GetTexture(path);
    if (texture == nullptr) {
        TextureDesc textureDesc = AssetManager::LoadImageFile(path);
        texture = TextureManager::CreateTexture(textureDesc);
        if (texture == nullptr) {
            LOG_ERROR("Error loading texture.");
        }
    }
    SetTexture(model, texture);
}

void SceneManager::AsyncLoadAndSetTexture(Model* model, std::filesystem::path path) {
    std::thread(LoadAndSetTexture, model, path).detach();
}
