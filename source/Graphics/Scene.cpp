#include "Luzpch.hpp"
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "Window.hpp"
#include "RayTracing.hpp"
#include "VulkanLayer.h"
#include "RenderingPass.hpp"

#include <imgui/imgui_stdlib.h>

namespace Scene {

void AcceptMeshPayload();
void AcceptTexturePayload(int& textureID);

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
    DeleteEntity(pointModel);
    DeleteEntity(dirModel);
    DeleteEntity(spotModel);

    Model* cube = AssetManager::LoadModel("assets/cube.glb");
    Model* plane = CreateModel(cube);
    plane->transform.SetPosition(glm::vec3(0, -1, 0));
    plane->transform.SetScale(glm::vec3(10, 0.0001, 10));
    Light* defaultLight = CreateLight();
    defaultLight->transform.SetPosition(glm::vec3(-5, 3, 3));
    defaultLight->block.intensity = 30;

    //AssetManager::LoadModels("assets/corvette_stingray.glb");

    // AssetManager::AsyncLoadModels("assets/ignore/sponza_pbr/sponza.glb");
    // AssetManager::LoadModels("assets/ignore/sponza_pbr/sponza.glb");

    // AssetManager::LoadModels("assets/ignore/sponza_pbr/Sponza.gltf");
    // AssetManager::AsyncLoadModels("assets/ignore/helmet/FlightHelmet.gltf");
    // AssetManager::LoadModels("assets/ignore/helmet/DamagedHelmet.gltf");
    // AssetManager::LoadModels("assets/ignore/helmet/SciFiHelmet.gltf");
    // AssetManager::LoadModels("assets/ignore/head/scene.gltf");
    // AssetManager::LoadModels("assets/ignore/sphere.glb");
    // AssetManager::AsyncLoadModels("assets/ignore/sponza/sponza_semitransparent.obj");
    // AssetManager::AsyncLoadModels("assets/cube.obj");
    // AssetManager::AsyncLoadModels("assets/ignore/coffee_cart/coffee_cart.obj");
    // AssetManager::AsyncLoadModels("assets/ignore/r3pu/r3pu.obj");
}

void CreateResources() {
    LUZ_PROFILE_FUNC();
    sceneBuffer = vkw::CreateBuffer(sizeof(Scene::scene), vkw::BufferUsage::Storage | vkw::BufferUsage::TransferDst);
    modelsBuffer = vkw::CreateBuffer(sizeof(Scene::models), vkw::BufferUsage::Storage | vkw::BufferUsage::TransferDst);
    u8* whiteTextureData = new u8[4];
    whiteTextureData[0] = 255;
    whiteTextureData[1] = 255;
    whiteTextureData[2] = 255;
    whiteTextureData[3] = 255;
    u8* blackTextureData = new u8[4];
    blackTextureData[0] = 0;
    blackTextureData[1] = 0;
    blackTextureData[2] = 0;
    blackTextureData[3] = 255;
    whiteTexture = vkw::CreateImage({
        .width = 1,
        .height = 1,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
        .name = "white texture"
    });
    blackTexture = vkw::CreateImage({
        .width = 1,
        .height = 1,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
        .name = "black texture"
    });
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdBarrier(whiteTexture, vkw::Layout::TransferDst);
    vkw::CmdBarrier(blackTexture, vkw::Layout::TransferDst);
    vkw::CmdCopy(whiteTexture, whiteTextureData, 4);
    vkw::CmdCopy(blackTexture, blackTextureData, 4);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdBarrier(whiteTexture, vkw::Layout::ShaderRead);
    vkw::CmdBarrier(blackTexture, vkw::Layout::ShaderRead);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);
}

void UpdateResources() {
    LUZ_PROFILE_FUNC();
    scene.whiteTexture = whiteTexture.rid;
    scene.blackTexture = blackTexture.rid;
    scene.numLights = 0;
    scene.viewSize = glm::vec2(vkw::ctx().swapChainExtent.width, vkw::ctx().swapChainExtent.height);
    for (int i = 0; i < modelEntities.size(); i++) {
        Model* model = modelEntities[i];
        model->id = i;
        models[i] = model->block;
        models[i].model = model->transform.GetMatrix();
        if (!model->useColorMap || model->block.material.colorMap == -1) {
            models[i].material.colorMap = whiteTexture.rid;
        } else {
            models[i].material.colorMap = AssetManager::images[model->block.material.colorMap].rid;
        }
        if (!model->useNormalMap || model->block.material.normalMap == -1) {
            models[i].material.normalMap = whiteTexture.rid;
        } else {
            models[i].material.normalMap = AssetManager::images[model->block.material.normalMap].rid;
        }
        if (!model->useMetallicRoughnessMap || model->block.material.metallicRoughnessMap == -1) {
            models[i].material.metallicRoughnessMap = whiteTexture.rid;
        } else {
            models[i].material.metallicRoughnessMap = AssetManager::images[model->block.material.metallicRoughnessMap].rid;
        }
        if (!model->useEmissionMap || model->block.material.emissionMap == -1) {
            models[i].material.emissionMap = blackTexture.rid;
        } else {
            models[i].material.emissionMap = AssetManager::images[model->block.material.emissionMap].rid;
        }
        if (!model->useAoMap || model->block.material.aoMap == -1) {
            models[i].material.aoMap = whiteTexture.rid;
        } else {
            models[i].material.aoMap = AssetManager::images[model->block.material.aoMap].rid;
        }
    }
    for (int i = 0; i < lightEntities.size(); i++) {
        int id = i + modelEntities.size();
        Light* light = lightEntities[i];
        light->id = id;
        models[id].model = light->transform.GetMatrix();
        models[id].material.color = glm::vec4(0, 0, 0, lightGizmosOpacity);
        models[id].material.emission = light->block.color * light->block.intensity;
        models[id].material.emissionMap = whiteTexture.rid;
        scene.lights[scene.numLights] = light->block;
        scene.lights[scene.numLights].position = light->transform.position;
        scene.lights[scene.numLights].direction = light->transform.GetGlobalFront();
        if (!light->shadows) {
            scene.lights[scene.numLights].numShadowSamples = 0;
        }
        scene.numLights++;
    }
    scene.camPos = camera.GetPosition();
    scene.projView = camera.GetProj() * camera.GetView();
    scene.inverseProj = glm::inverse(camera.GetProj());
    scene.inverseView = glm::inverse(camera.GetView());
    RayTracing::CreateTLAS();
}

void DestroyResources() {
    sceneBuffer = {};
    modelsBuffer = {};
    whiteTexture = {};
    blackTexture = {};
}

Model* CreateModel() {
    Model* model = new Model();
    model->name = "Model";
    model->mesh = 0;
    model->entityType = EntityType::Model;
    entities.push_back(model);
    modelEntities.push_back(model);
    SetCollection(model, rootCollection);
    RayTracing::SetRecreateTLAS();
    return model;
}

Model* CreateModel(Model* copy) {
    Model* entity = new Model();

    entity->name = copy->name;
    entity->transform = copy->transform;
    entity->entityType = EntityType::Model;
    entities.push_back(entity);
    SetCollection(entity, copy->parent);

    entity->mesh = copy->mesh;
    entity->block = copy->block;
    entity->useAoMap = copy->useAoMap;
    entity->useColorMap = copy->useColorMap;
    entity->useNormalMap = copy->useNormalMap;
    entity->useEmissionMap = copy->useEmissionMap;
    entity->useMetallicRoughnessMap = copy->useMetallicRoughnessMap;
    modelEntities.push_back(entity);
    RayTracing::SetRecreateTLAS();
    return entity;
}

Light* CreateLight() {
    Light* light = new Light();
    light->name = "Light";
    light->entityType = EntityType::Light;
    light->block.type = LightType::Point;
    light->transform.SetScale(glm::vec3(0.1));
    entities.push_back(light);
    lightEntities.push_back(light);
    SetCollection(light, rootCollection);
    return light;
}

Light* CreateLight(Light* copy) {
    Light* entity = new Light();

    entity->name = copy->name;
    entity->transform = copy->transform;
    entity->entityType = EntityType::Light;
    entities.push_back(entity);
    SetCollection(entity, copy->parent);

    entity->block = copy->block;
    lightEntities.push_back(entity);
    return entity;
}

Collection* CreateCollection() {
    Collection* collection = new Collection();
    collection->entityType = EntityType::Collection;
    collection->name = "Collection";
    SetCollection(collection, rootCollection);
    entities.push_back(collection);
    return collection;
}

Collection* CreateCollection(Collection* copy) {
    Collection* collection = new Collection();

    collection->name = copy->name;
    collection->transform = copy->transform;
    collection->entityType = EntityType::Collection;
    entities.push_back(collection);
    SetCollection(collection, copy->parent);

    for (int i = 0; i < copy->children.size(); i++) {
        Entity* entity = CreateEntity(copy->children[i]);
        SetCollection(entity, collection);
    }
    return collection;
}

Entity* CreateEntity(Entity* copy) {
    Entity* entity = nullptr;
    if (copy->entityType == EntityType::Collection) { 
        entity = CreateCollection((Collection*)copy); 
    } else if (copy->entityType == EntityType::Model) { 
        entity = CreateModel((Model*)copy); 
    } else if (copy->entityType == EntityType::Light) { 
        entity = CreateLight((Light*)copy); 
    }
    DEBUG_ASSERT(entity != nullptr, "Entity type invalid during copy.");
    return entity;
}

void DeleteCollection(Collection* collection) {
    while (collection->children.size()) {
        DeleteEntity(*collection->children.begin());
    }
}

void DeleteEntity(Entity* entity) {
    if (selectedEntity == entity) {
        selectedEntity = nullptr;
    }
    if (copiedEntity == entity) {
        copiedEntity = nullptr;
    }
    if (entity->parent != nullptr) {
        RemoveFromCollection(entity);
    }
    if (entity->entityType == EntityType::Collection) {
        DeleteCollection((Collection*)entity);
    } else if (entity->entityType == EntityType::Model) {
        RayTracing::SetRecreateTLAS();
        auto it = std::find(modelEntities.begin(), modelEntities.end(), (Model*)entity);
        DEBUG_ASSERT(it != modelEntities.end(), "Model isn't on the vector of models.");
        modelEntities.erase(it);
    } else if (entity->entityType == EntityType::Light) {
        auto it = std::find(lightEntities.begin(), lightEntities.end(), (Light*)entity);
        DEBUG_ASSERT(it != lightEntities.end(), "Model isn't on the vector of models.");
        lightEntities.erase(it);
    }
    auto it = std::find(entities.begin(), entities.end(), entity);
    DEBUG_ASSERT(it != entities.end(), "Entity isn't on the vector of entities.");
    entities.erase(it);
    delete entity;
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
    bool controlPressed = Window::IsKeyDown(GLFW_KEY_LEFT_CONTROL) || Window::IsKeyPressed(GLFW_KEY_LEFT_CONTROL);
    if (selectedEntity) {
        if(controlPressed && Window::IsKeyPressed(GLFW_KEY_C)) {
            copiedEntity = selectedEntity;
        }
        if (Window::IsKeyPressed(GLFW_KEY_X)) {
            DeleteEntity(selectedEntity);
        }
    }
    if(controlPressed && Window::IsKeyPressed(GLFW_KEY_V)) {
        if (copiedEntity != nullptr) {
            selectedEntity = CreateEntity(copiedEntity);
        }
    }
    if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
        AcceptMeshPayload();
        OnImgui(rootCollection, true);
    }
    ImGui::Separator();
    if (ImGui::Button("Add model")) {
        selectedEntity = CreateModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add light")) {
        selectedEntity = CreateLight();
    }
    if (ImGui::CollapsingHeader("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Color", glm::value_ptr(scene.ambientLightColor));
        ImGui::DragFloat("Intensity", &scene.ambientLightIntensity, 0.01);
    }
    if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &aoActive);
        ImGui::DragInt("Samples", &aoNumSamples, 1, 1);
        ImGui::DragFloat("Min", &scene.aoMin, 0.0001, 0.0f, 1.0f, "%.8f", ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Max", &scene.aoMax, 0.01, 0.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Power", &scene.aoPower, 0.001, 0.0f, 100.0f, "%.4f");
        scene.aoNumSamples = aoActive ? aoNumSamples : 0;
    }
    {
        bool active = scene.useBlueNoise == 1;
        ImGui::Checkbox("Blue noise", &active);
        scene.useBlueNoise = active ? 1 : 0;
        if (ImGui::Button("Clear thrash")) {
            RenderingPassManager::DestroyTrash();
        }
    }
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
            ImGui::PushID(entity);
            if (entity->entityType == EntityType::Collection) {
                OnImgui((Collection*)entity);
            }
            else {
                if (ImGui::Selectable(entity->name.c_str(), selectedEntity == entity)) {
                    selectedEntity = entity;
                }
            }
            ImGui::PopID();

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
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNode("Color")) {
            ImGui::ColorEdit3("Color", glm::value_ptr(model->block.material.color));
            ImGui::Checkbox("Use texture", &model->useColorMap);
            DrawTextureOnImgui(AssetManager::images[model->block.material.colorMap]);
            AcceptTexturePayload(model->block.material.colorMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Normal")) {
            ImGui::Checkbox("Use texture", &model->useNormalMap);
            DrawTextureOnImgui(AssetManager::images[model->block.material.normalMap]);
            AcceptTexturePayload(model->block.material.normalMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Metallic Roughness")) {
            ImGui::DragFloat("Metallic", &model->block.material.metallic, 0.001, 0, 1);
            ImGui::DragFloat("Roughness", &model->block.material.roughness, 0.001, 0, 1);
            ImGui::Checkbox("Use texture", &model->useMetallicRoughnessMap);
            DrawTextureOnImgui(AssetManager::images[model->block.material.metallicRoughnessMap]);
            AcceptTexturePayload(model->block.material.metallicRoughnessMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Emission")) {
            ImGui::ColorEdit3("Emission", glm::value_ptr( model->block.material.emission));
            ImGui::Checkbox("Use texture", &model->useEmissionMap);
            DrawTextureOnImgui(AssetManager::images[model->block.material.emissionMap]);
            AcceptTexturePayload(model->block.material.emissionMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Ambient Occlusion")) {
            ImGui::Checkbox("Use texture", &model->useAoMap);
            DrawTextureOnImgui(AssetManager::images[model->block.material.aoMap]);
            AcceptTexturePayload(model->block.material.aoMap);
            ImGui::TreePop();
        }
    }
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
    if (ImGui::CollapsingHeader("Shadows")) {
        ImGui::Checkbox("Active", &light->shadows);
        ImGui::DragInt("Num samples", (int*) &light->block.numShadowSamples, 1, 1, 64);
        ImGui::DragFloat("Radius", &light->block.radius, 0.01, 0.0f);
        light->block.radius = std::max(light->block.radius, 0.0f);
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

    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
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

void AcceptMeshPayload() {
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("mesh");
        if (payload) {
            std::string path((const char*)payload->Data, payload->DataSize);
            AssetManager::AsyncLoadModels(path);
        }
    }
}

void AcceptTexturePayload(int& textureID) {
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payloadTexture = ImGui::AcceptDragDropPayload("texture");
        if (payloadTexture) {
            std::string path((const char*)payloadTexture->Data, payloadTexture->DataSize);
            textureID = AssetManager::LoadTexture(path);
        }
        const ImGuiPayload* payloadTextureID = ImGui::AcceptDragDropPayload("textureID");
        if (payloadTextureID) {
            textureID = *(int*)payloadTextureID->Data;
        }
    }
}

}
