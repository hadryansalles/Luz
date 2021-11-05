#include "Luzpch.hpp"

#include "SceneManager.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "SwapChain.hpp"
#include "UnlitGraphicsPipeline.hpp"
#include "PhongGraphicsPipeline.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include "Camera.hpp"
#include "Window.hpp"

#include <future>

void SceneManager::Setup() {
    mainCollection.name = "Root";

    // std::vector<ModelDesc> newModels;
    // newModels = AssetManager::LoadMeshFile("assets/ignore/sponza/sponza_semitransparent.obj");
    // for (auto& modelDesc : newModels) {
    //     Model* model = CreateModel(modelDesc);
    //     model->material.type = MaterialType::Phong;
    //     AddModel(model, modelDesc.collection);
    // }

    // ModelDesc modelDesc = AssetManager::LoadMeshFile("assets/cube.obj")[0];
    // Model* model = CreateModel(modelDesc);
    // model->material.type = MaterialType::Phong;
    // AddModel(model, modelDesc.collection);

    AddLight(LightManager::CreateLight());
    // AsyncLoadModels("assets/ignore/dragon.obj");
    AsyncLoadModels("assets/cube.obj");
    AsyncLoadModels("assets/cube.obj");
    // AsyncLoadModels("assets/teapot.obj");
    // AsyncLoadModels("assets/ignore/sponza/sponza_semitransparent.obj");
}

void SceneManager::LoadModels(std::filesystem::path path, Collection* collection) {
    AssetManager::AddObjFileToScene(path, collection);
}

void SceneManager::AsyncLoadModels(std::filesystem::path path, Collection* collection) {
    std::thread(LoadModels, path, collection).detach();
}

void CreateModelDescriptors(Model* model) {
    // model->meshDescriptor = UnlitGraphicsPipeline::CreateModelDescriptor();
    // model->material.materialDescriptor = UnlitGraphicsPipeline::CreateMaterialDescriptor();
    // GraphicsPipelineManager::UpdateBufferDescriptor(model->meshDescriptor, &model->ubo, sizeof(model->ubo));
    // GraphicsPipelineManager::UpdateBufferDescriptor(model->material.materialDescriptor, (UnlitMaterialUBO*)&model->material, sizeof(UnlitMaterialUBO));
}

void SceneManager::AddPreloadedModel(ModelDesc desc) {
    preloadedModelsLock.lock();
    preloadedModels.push_back(desc);
    preloadedModelsLock.unlock();
}

void SceneManager::Create() {
    for (Model* model : models) {
        CreateModelDescriptors(model);
    }
}

void SceneManager::Destroy() {
    for (Model* model : models) {
        for (BufferResource& buffer : model->meshDescriptor.buffers) {
            BufferManager::Destroy(buffer);
        }
        for (BufferResource& buffer : model->material.materialDescriptor.buffers) {
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

Model* SceneManager::CreateModel(ModelDesc& desc) {
    Model* model = new Model();
    model->mesh = MeshManager::CreateMesh(desc.mesh);
    model->name = desc.mesh->name;
    model->id = modelID++;
    model->transform.position += model->mesh->center;
    CreateModelDescriptors(model);
    if (desc.texture.data != nullptr) {
        model->material.diffuseTexture = TextureManager::GetTexture(desc.texture.path);
        if (model->material.diffuseTexture == 0) {
            model->material.diffuseTexture = TextureManager::CreateTexture(desc.texture);
        }
    }
    else {
        model->material.diffuseTexture = 0;
    }
    return model;
}

Model* SceneManager::CreateGizmoModel(MeshResource* mesh) {
    Model* model = new Model();
    model->mesh = mesh;
    model->transform.position += model->mesh->center;
    model->material.type = MaterialType::Unlit;
    CreateModelDescriptors(model);
    return model;
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

void SceneManager::LightOnImgui(Light* light) {
    bool selected = light == selectedLight;
    ImGui::PushID(light->id);

    selected = ImGui::Selectable(light->name.c_str(), &selected);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        SelectLight(light);
        openSceneItemMenu = true;
    }

    if(selected) {
        SelectLight(light);
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("light", &light, sizeof(Light*));
        ImGui::Text(light->name.c_str());
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

    CollectionDragDropTarget(collection);

    if(open) {
        for (int i = 0; i < collection->children.size(); i++) {
            CollectionOnImgui(collection->children[i], i);
        }
        for (int i = 0; i < collection->models.size(); i++) {
            ModelOnImgui(collection->models[i]);
        }
        for (int i = 0; i < collection->lights.size(); i++) {
            LightOnImgui(collection->lights[i]);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneManager::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    if (ImGui::CollapsingHeader("Collections", ImGuiTreeNodeFlags_DefaultOpen)) {
        CollectionDragDropTarget(&mainCollection);
        openSceneItemMenu &= !ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        for (int i = 0; i < mainCollection.children.size(); i++) {
            CollectionOnImgui(mainCollection.children[i], i);
        }
        for (int i = 0; i < mainCollection.models.size(); i++) {
            ModelOnImgui(mainCollection.models[i]);
        }
        for (int i = 0; i < mainCollection.lights.size(); i++) {
            LightOnImgui(mainCollection.lights[i]);
        }
        bool controlPressed = Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) || Window::IsKeyPressed(GLFW_KEY_LEFT_CONTROL);
        if (controlPressed && Window::IsKeyPressed(GLFW_KEY_C)) {
            if (selectedCollection != nullptr && selectedModel != nullptr) {
                LOG_WARN("Selected model and collection simultaneously");
            }
            if (selectedCollection != nullptr && selectedCollection != &mainCollection) {
                SetCopiedCollection(selectedCollection);
            } else if (selectedModel != nullptr) {
                SetCopiedModel(selectedModel);
            } else if (selectedLight != nullptr) {
                SetCopiedLight(selectedLight);
            }
        }
        if (controlPressed && Window::IsKeyPressed(GLFW_KEY_V)) {
            if (copiedCollection != nullptr && copiedModel != nullptr) {
                LOG_WARN("Copied model and collection simultaneously");
            }
            if (copiedCollection != nullptr) {
                Collection* collection = CreateCollectionCopy(copiedCollection);
                SelectCollection(collection);
            } else if (copiedModel != nullptr) {
                Model* model = AddModelCopy(copiedModel);
                if (selectedCollection != nullptr) {
                    SetCollection(model, selectedCollection);
                }
                else {
                    SetCollection(model, copiedModel->collection);
                }
                SelectModel(model);
            } else if (copiedLight != nullptr) {
                Light* copy = LightManager::CreateLightCopy(copiedLight);
                AddLight(copy, copiedLight->collection);
                SelectLight(copy);
            }
        }
        if (Window::IsKeyPressed(GLFW_KEY_X)) {
            if (selectedModel != nullptr) {
                DeleteModel(selectedModel);
            }
            if (selectedCollection != nullptr) {
                DeleteCollection(selectedCollection);
            }
        }
        if (openSceneItemMenu) {
            ImGui::OpenPopup("right_click_hierarchy");
        }
        if (ImGui::BeginPopup("right_click_hierarchy")) {
            Collection* parentCollection = &mainCollection;
            if (selectedModel != nullptr) {
                parentCollection = selectedModel->collection;
            }
            else if (selectedLight != nullptr) {
                parentCollection = selectedLight->collection;
            }
            if(selectedLight != nullptr || selectedModel != nullptr) {
                if (ImGui::MenuItem("New collection")) {
                    CreateCollection(selectedModel->collection);
                }
                if (ImGui::MenuItem("New light")) {
                    Light* newLight = LightManager::CreateLight();
                    AddLight(newLight, parentCollection);
                    SelectLight(newLight);
                }
                if (ImGui::MenuItem("Copy")) {
                    if(selectedModel != nullptr) { 
                        SetCopiedModel(selectedModel);
                    } else if (selectedLight != nullptr) {
                        SetCopiedLight(selectedLight);
                    }
                }
                if (ImGui::MenuItem("Delete")) {
                    if(selectedModel != nullptr) { 
                        DeleteModel(selectedModel);
                    } else if (selectedLight != nullptr) {
                        DeleteLight(selectedLight);
                    }
                }
            }
            if (selectedCollection != nullptr) {
                if (ImGui::MenuItem("New collection")) {
                    CreateCollection(selectedCollection);
                }
                if (ImGui::MenuItem("New light")) {
                    Light* newLight = LightManager::CreateLight();
                    AddLight(newLight);
                    SelectLight(newLight);
                }
                if (copiedModel != nullptr) {
                    if (ImGui::MenuItem("Paste")) {
                        Model* copy = AddModelCopy(copiedModel);
                        SetCollection(copy, selectedCollection);
                        SelectModel(copy);
                    }
                }
                if (copiedCollection != nullptr) {
                    if (ImGui::MenuItem("Paste")) {
                        Light* copy = LightManager::CreateLightCopy(copiedLight);
                        AddLight(copy, copiedLight->collection);
                        SelectLight(copy);
                    }
                }
                if (copiedCollection != nullptr) {
                    if (ImGui::MenuItem("Paste")) {
                        Collection* copy = CreateCollectionCopy(copiedCollection, selectedCollection);
                        SelectCollection(copy);
                    }
                }
            }
            if (selectedCollection != nullptr && selectedCollection != &mainCollection) {
                if (ImGui::MenuItem("Copy")) {
                    SetCopiedCollection(selectedCollection);
                }
                if (ImGui::MenuItem("Delete")) {
                    DeleteCollection(selectedCollection);
                }
            }
            ImGui::EndPopup();
        }
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
    if (selectedCollection == collection) {
        SelectCollection(nullptr);
    }
    if (copiedCollection == selectedCollection) {
        copiedCollection = nullptr;
    }
    auto it = std::find(collections.begin(), collections.end(), collection);
    if (it == collections.end()) {
        LOG_ERROR("Trying to delete invalid collection from scene...");
    }
    collections.erase(it);
    RemoveCollectionFromParent(collection);
    for (int i = 0; i < collection->children.size(); i++) {
        DeleteCollection(collection->children[i]);
    }
    while (!collection->models.empty()) {
        DeleteModel(*collection->models.begin());
    }
    while (!collection->lights.empty()) {
        LightManager::DestroyLight(*collection->lights.begin());
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

void SceneManager::DestroyModel(Model* model) {
    vkDeviceWaitIdle(LogicalDevice::GetVkDevice());

    for (BufferResource& buffer : model->meshDescriptor.buffers) {
        BufferManager::Destroy(buffer);
    }
    for (BufferResource& buffer : model->material.materialDescriptor.buffers) {
        BufferManager::Destroy(buffer);
    }
    delete model;
}

void SceneManager::DeleteModel(Model* model) {
    vkDeviceWaitIdle(LogicalDevice::GetVkDevice());

    if (selectedModel == model) {
        SelectModel(nullptr);
    }
    if (copiedModel == model) {
        copiedModel = nullptr;
    }

    DeleteModelFromCollection(model);

    auto it = std::find(models.begin(), models.end(), model);
    if (it == models.end()) {
        LOG_ERROR("Trying to delete invalid model from scene...");
        return;
    }
    models.erase(it);

    for (BufferResource& buffer : model->meshDescriptor.buffers) {
        BufferManager::Destroy(buffer);
    }
    for (BufferResource& buffer : model->material.materialDescriptor.buffers) {
        BufferManager::Destroy(buffer);
    }

    delete model;
}

void SceneManager::DeleteLight(Light* light) {
    vkDeviceWaitIdle(LogicalDevice::GetVkDevice());

    if (selectedLight == light) {
        SelectModel(nullptr);
    }
    if (copiedLight == light) {
        copiedModel = nullptr;
    }

    DeleteLightFromCollection(light);

    LightManager::DestroyLight(light);
}

void SceneManager::SelectCollection(Collection* collection) {
    selectedModel = nullptr;
    selectedLight = nullptr;
    selectedCollection = collection;
    selectedTransform = collection != nullptr ? &collection->transform : nullptr;
}

void SceneManager::SelectModel(Model* model) {
    selectedCollection = nullptr;
    selectedLight = nullptr;
    selectedModel = model;
    selectedTransform = model != nullptr ? &model->transform : nullptr;
}

void SceneManager::SelectLight(Light* light) {
    selectedCollection = nullptr;
    selectedModel = nullptr;
    selectedLight = light;
    selectedTransform = light != nullptr ? &light->transform : nullptr;
}

void SceneManager::SetCopiedCollection(Collection* collection) {
    copiedModel = nullptr;
    copiedCollection = collection;
}

void SceneManager::SetCopiedModel(Model* model) {
    copiedCollection = nullptr;
    copiedModel = model;
    copiedLight = nullptr;
}

void SceneManager::SetCopiedLight(Light* light) {
    copiedCollection = nullptr;
    copiedModel = nullptr;
    copiedLight = light;
}

void SceneManager::LoadAndSetTexture(Model* model, std::filesystem::path path) {
    RID texture = TextureManager::GetTexture(path);
    if (texture == 0) {
        TextureDesc textureDesc = AssetManager::LoadImageFile(path);
        texture = TextureManager::CreateTexture(textureDesc);
        if (texture == 0) {
            LOG_ERROR("Error loading texture.");
        }
    }
    model->material.diffuseTexture = texture;
}

void SceneManager::AsyncLoadAndSetTexture(Model* model, std::filesystem::path path) {
    std::thread(LoadAndSetTexture, model, path).detach();
}

Collection* SceneManager::CreateCollectionCopy(Collection* copy, Collection* parent) {
    Collection* collection = CreateCollection(parent);
    collection->name = copy->name;
    collection->transform = copy->transform;
    for (Model* model : copy->models) {
        SetCollection(AddModelCopy(model), collection);
    }
    return collection;
}

Model* SceneManager::CreateModelCopy(const Model* copy) {
    Model* model = new Model();
    model->mesh = copy->mesh;
    model->name = copy->name;
    model->transform = copy->transform;
    model->material = copy->material;
    CreateModelDescriptors(model);
    return model;
}

Model* SceneManager::AddModelCopy(const Model* copy) {
    Model* model = CreateModelCopy(copy);
    model->id = modelID++;
    models.push_back(model);
    SetCollection(model, nullptr);
    return model;
}

void SceneManager::CollectionDragDropTarget(Collection* collection) {
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

        const ImGuiPayload* lightPayload = ImGui::AcceptDragDropPayload("light");
        if (lightPayload) {
            Light* light = *(Light**)lightPayload->Data;
            SetCollection(light, collection);
            ImGui::EndDragDropTarget();
        }

        const ImGuiPayload* collectionPayload = ImGui::AcceptDragDropPayload("collection");
        if (collectionPayload) {
            Collection* childCollection = *(Collection**) collectionPayload->Data;
            SetCollectionParent(childCollection, collection);
            ImGui::EndDragDropTarget();
        }
    }
}

void SceneManager::AddLight(Light* light, Collection* collection) {
    if (collection == nullptr) {
        collection = &mainCollection;
    }
    light->collection = collection;
    light->transform.parent = &collection->transform;
    collection->lights.push_back(light);
}

void SceneManager::DeleteLightFromCollection(Light* light) {
    Collection* oldCollection = light->collection;
    auto it = std::find(oldCollection->lights.begin(), oldCollection->lights.end(), light);
    DEBUG_ASSERT(it != oldCollection->lights.end(), "Light not found inside collection!");
    oldCollection->lights.erase(it);
}

void SceneManager::SetCollection(Light* light, Collection* collection) {
    DeleteLightFromCollection(light);
    if (collection == nullptr) {
        collection = &mainCollection;
    }
    light->collection = collection;
    light->transform.parent = &collection->transform;
    collection->lights.push_back(light);
}
