#include "Luzpch.hpp"
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"

#include <imgui/imgui_stdlib.h>

namespace Scene {
 
void Setup() {
    LUZ_PROFILE_FUNC();
    rootCollection = new Collection();
    rootCollection->entityType = EntityType::Collection;
    rootCollection->name = "Root";

    auto pointModel = AssetManager::LoadModel("assets/point.obj");
    auto dirModel = AssetManager::LoadModel("assets/directional.obj");
    auto spotModel = AssetManager::LoadModel("assets/spot.obj");
    Scene::lightMeshes[0] = pointModel->mesh;
    Scene::lightMeshes[1] = dirModel->mesh;
    Scene::lightMeshes[2] = spotModel->mesh;
    DeleteModel(pointModel);
    DeleteModel(dirModel);
    DeleteModel(spotModel);

    AssetManager::AsyncLoadModels("assets/ignore/sponza/sponza_semitransparent.obj");
    AssetManager::AsyncLoadModels("assets/ignore/dragon.obj");
}

void CreateResources() {
    LUZ_PROFILE_FUNC();
    BufferManager::CreateStorageBuffer(Scene::sceneBuffer, sizeof(Scene::scene));
    BufferManager::CreateStorageBuffer(Scene::modelsBuffer, sizeof(Scene::models));
    GraphicsPipelineManager::WriteStorage(Scene::sceneBuffer, SCENE_BUFFER_INDEX);
    GraphicsPipelineManager::WriteStorage(Scene::modelsBuffer, MODELS_BUFFER_INDEX);
}

void UpdateBuffers(int numFrame) {
    LUZ_PROFILE_FUNC();
    BufferManager::UpdateStorage(sceneBuffer, numFrame, &scene);
    BufferManager::UpdateStorage(modelsBuffer, numFrame, &models);
}

void UpdateResources(int numFrame) {
    LUZ_PROFILE_FUNC();
    scene.numLights = 0;
    for (int i = 0; i < modelEntities.size(); i++) {
        Model* model = modelEntities[i];
        model->id = i;
        models[i] = model->block;
        models[i].model = model->transform.GetMatrix();
    }
    for (int i = 0; i < lightEntities.size(); i++) {
        int id = i + modelEntities.size();
        Light* light = lightEntities[i];
        light->id = id;
        models[id].model = light->transform.GetMatrix();
        models[id].colors[0] = glm::vec4(light->block.color, lightGizmosOpacity);
        models[id].textures[0] = 1;
        scene.lights[scene.numLights] = light->block;
        scene.lights[scene.numLights].position = light->transform.position;
        scene.lights[scene.numLights].direction = light->transform.GetGlobalFront();
        scene.numLights++;
    }
    scene.projView = camera.GetProj() * camera.GetView();
    UpdateBuffers(numFrame);
}

void DestroyResources() {
    BufferManager::DestroyStorageBuffer(sceneBuffer);
    BufferManager::DestroyStorageBuffer(modelsBuffer);
}

Model* CreateModel() {
    Model* model = new Model();
    model->name = "Model";
    model->mesh = 0;
    model->entityType = EntityType::Model;
    model->block.textures[0] = 0;
    entities.push_back(model);
    modelEntities.push_back(model);
    SetCollection(model, rootCollection);
    return model;
}

Model* CreateModel(Model& copy) {
    Model* model = new Model();
    model->name = copy.name;
    model->block = copy.block;
    model->mesh = copy.mesh;
    model->transform = copy.transform;
    model->material = copy.material;
    model->entityType = EntityType::Model;
    entities.push_back(model);
    modelEntities.push_back(model);
    SetCollection(model, copy.parent);
    return model;
}

Light* CreateLight() {
    Light* light = new Light();
    light->name = "Light";
    light->entityType = EntityType::Light;
    light->block.type = LightType::Point;
    entities.push_back(light);
    lightEntities.push_back(light);
    SetCollection(light, rootCollection);
    return light;
}

Collection* CreateCollection() {
    Collection* collection = new Collection();
    collection->entityType = EntityType::Collection;
    collection->name = "Collection";
    SetCollection(collection, rootCollection);
    entities.push_back(collection);
    return collection;
}

void DeleteModel(Model* model) {
    if (model->parent != nullptr) {
        RemoveFromCollection(model);
    }
    {
        auto it = std::find(entities.begin(), entities.end(), model);
        DEBUG_ASSERT(it != entities.end(), "Model isn't on the vector of entities.");
        entities.erase(it);
    }
    {
        auto it = std::find(modelEntities.begin(), modelEntities.end(), model);
        DEBUG_ASSERT(it != modelEntities.end(), "Model isn't on the vector of models.");
        modelEntities.erase(it);
    }
    delete model;
}

void RemoveFromCollection(Entity* entity) {
    Collection* parent = entity->parent;
    auto it = std::find(parent->children.begin(), parent->children.end(), entity);
    DEBUG_ASSERT(it != parent->children.end(), "Entity isn't a children of its parent.");
    parent->children.erase(it);
    entity->parent = nullptr;
    entity->transform.parent = nullptr;
}

void SetCollection(Entity* entity, Collection* collection) {
    if (entity->parent != nullptr) {
        RemoveFromCollection(entity);
    }
    if (collection == nullptr) {
        collection = rootCollection;
    }
    entity->parent = collection;
    entity->transform.parent = &collection->transform;
    collection->children.push_back(entity);
}

void OnImgui() {
    if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
        OnImgui(rootCollection, true);
    }
    if (ImGui::Button("Add model")) {
        selectedEntity = CreateModel();
    }
    if (ImGui::Button("Add light")) {
        selectedEntity = CreateLight();
    }
    ImGui::ColorEdit3("Ambient color", glm::value_ptr(scene.ambientLightColor));
    ImGui::DragFloat("Ambient intensity", &scene.ambientLightIntensity, 0.01);
}

void OnImgui(Collection* collection, bool root) {
    bool open = true;
    if (!root) {
        ImGui::PushID(collection);
        open = ImGui::TreeNode(collection->name.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedEntity = collection;
        }
    }
    if (open) {
        for (Entity* entity : collection->children) {
            if (entity->entityType == EntityType::Collection) {
                OnImgui((Collection*)entity);
            }
            else {
                if (ImGui::Selectable(entity->name.c_str(), selectedEntity == entity)) {
                    selectedEntity = entity;
                }
            }

        }
        if (!root) {
            ImGui::TreePop();
        }
    }
    if (!root) {
        ImGui::PopID();
    }
}

void InspectModel(Model* model) {
    ImGui::Text("Mesh: %d", model->mesh);
}

void InspectLight(Light* light) {
    static const char* LIGHT_NAMES[] = { "Point", "Directional", "Spot" };
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Type", LIGHT_NAMES[light->block.type])) {
            for (int i = 0; i < 3; i++) {
                if (ImGui::Selectable(LIGHT_NAMES[i], light->block.type == i)) {
                    light->block.type = (LightType)i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::ColorEdit3("Color", glm::value_ptr(light->block.color));
        ImGui::DragFloat("Intensity", &light->block.intensity);
        if (light->block.type == LightType::Spot) {
            float innerAngle = glm::degrees(light->block.innerAngle);
            float outerAngle = glm::degrees(light->block.outerAngle);
            ImGui::DragFloat("Inner Angle", &innerAngle);
            ImGui::DragFloat("Outer Angle", &outerAngle);
            light->block.innerAngle = glm::radians(innerAngle);
            light->block.outerAngle = glm::radians(outerAngle);
        }
    }
}

void InspectEntity(Entity* entity) {
    ImGui::InputText("Name", &entity->name);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", glm::value_ptr(entity->transform.position));
        ImGui::DragFloat3("Scale", glm::value_ptr(entity->transform.scale));
        ImGui::DragFloat3("Rotation", glm::value_ptr(entity->transform.rotation));
        RenderTransformGizmo(entity->transform);
    }
    if (entity->entityType == EntityType::Model) {
        InspectModel((Model*)entity);
    } else if (entity->entityType == EntityType::Light) {
        InspectLight((Light*)entity);
    }
}

void RenderTransformGizmo(Transform& transform) {
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
    glm::mat4 guizmoProj(camera.GetProj());
    guizmoProj[1][1] *= -1;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(camera.GetView()), glm::value_ptr(guizmoProj), currentGizmoOperation,
    currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

    if (transform.parent != nullptr) {
        modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
    }
    transform.transform = modelMat;
    
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.transform), glm::value_ptr(transform.position), 
        glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
}

}
