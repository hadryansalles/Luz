#include "Luzpch.hpp"
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "Window.hpp"
#include "RayTracing.hpp"
#include "SwapChain.hpp"
#include "ImageManager.hpp"
#include "LogicalDevice.hpp"

#include <imgui/imgui_stdlib.h>

void AcceptTexturePayload(RID& textureID);

void Scene::Setup() {
    LUZ_PROFILE_FUNC();
    //rootCollection = new Collection();
    //rootCollection->entityType = EntityType::Collection;
    //rootCollection->name = "Root";

    //auto pointModel = AssetManager::LoadModel("assets/point.obj", *this);
    //auto dirModel = AssetManager::LoadModel("assets/directional.obj", *this);
    //auto spotModel = AssetManager::LoadModel("assets/spot.obj", *this);
    //Scene::lightMeshes[0] = pointModel->mesh;
    //Scene::lightMeshes[1] = dirModel->mesh;
    //Scene::lightMeshes[2] = spotModel->mesh;
    //DeleteEntity(pointModel);
    //DeleteEntity(dirModel);
    //DeleteEntity(spotModel);

    Entity* cube = AssetManager::LoadModel("assets/cube.glb", *this);
    Entity* vayne = AssetManager::LoadModel("assets/vayne2.gltf", *this);
    cube->transform.SetScale(glm::vec3(10.0f, 0.001f, 10.0f));
    //vayne->transform.SetScale(glm::vec3(0.05f));
    //Model* plane = CreateModel(cube);
    //plane->transform.SetPosition(glm::vec3(0, -1, 0));
    //plane->transform.SetScale(glm::vec3(10, 0.0001, 10));

    Light* defaultLight = CreateLight();
    defaultLight->transform.SetPosition(glm::vec3(-5, 3, 3));
    renderLightGizmos = false;
    defaultLight->block.intensity = 30;
    defaultLight->block.type = LightType::Point;
    defaultLight->transform.SetRotation(glm::vec3(45, 0, 0));

    //AssetManager::LoadModel("assets/scene.gltf", *this);
    //dragon->transform.SetPosition(glm::vec3(0, 1.026, 0));
    //dragon->transform.SetScale(glm::vec3(0.05));
    //dragon->transform.SetRotation(glm::vec3(90, 0, 0));

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

void Scene::CreateResources() {
    LUZ_PROFILE_FUNC();
    BufferManager::CreateStorageBuffer(sceneBuffer, sizeof(scene));
    BufferManager::CreateStorageBuffer(modelsBuffer, sizeof(models));
    GraphicsPipelineManager::WriteStorage(sceneBuffer, SCENE_BUFFER_INDEX);
    GraphicsPipelineManager::WriteStorage(modelsBuffer, MODELS_BUFFER_INDEX);
    {
        u32 sz = shadowMapSize;
        u32 mipLevels = 1;
        ImageDesc imageDesc = {};
        imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.width = shadowMapSize;
        imageDesc.height = shadowMapSize;
        imageDesc.mipLevels = mipLevels;
        imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.format = VK_FORMAT_D32_SFLOAT;
        imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        imageDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageDesc.size = (u64) sz * sz;
        imageDesc.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (int i = 0; i < MAX_LIGHTS; i++) {
            ImageManager::Create(imageDesc, shadowMaps[i].image);
            CreateSamplerAndImgui(mipLevels, shadowMaps[i]);
            VkCommandBuffer cmd = LogicalDevice::BeginSingleTimeCommands();
            ImageManager::BarrierDepthAttachmentToRead(cmd, shadowMaps[i].image.image);
        }
    }
}

void Scene::UpdateBuffers(int numFrame) {
    LUZ_PROFILE_FUNC();
    BufferManager::UpdateStorage(sceneBuffer, numFrame, &scene);
    BufferManager::UpdateStorage(modelsBuffer, numFrame, &models);
}

void Scene::UpdateResources(int numFrame) {
    LUZ_PROFILE_FUNC();
    scene.numLights = 0;
    scene.viewSize = glm::vec2(SwapChain::GetExtent().width, SwapChain::GetExtent().height);
    for (int i = 0; i < modelEntities.size(); i++) {
        Model* model = modelEntities[i];
        model->id = i;
        models[i] = model->block;
        models[i].model = model->transform.GetMatrix();
        if (!model->useColorMap) {
            models[i].material.colorMap = 0;
        }
        if (!model->useNormalMap) {
            models[i].material.normalMap = 0;
        }
        if (!model->useMetallicRoughnessMap) {
            models[i].material.metallicRoughnessMap = 0;
        }
        if (!model->useEmissionMap) {
            models[i].material.emissionMap = 0;
        }
        if (!model->useAoMap) {
            models[i].material.aoMap = 0;
        }
    }
    for (int i = 0; i < lightEntities.size(); i++) {
        int id = i + modelEntities.size();
        Light* light = lightEntities[i];
        light->id = id;
        models[id].model = light->transform.GetMatrix();
        models[id].material.color = glm::vec4(0, 0, 0, lightGizmosOpacity);
        models[id].material.emission = light->block.color * light->block.intensity;
        models[id].material.emissionMap = 0;
        scene.lights[scene.numLights] = light->block;
        scene.lights[scene.numLights].position = light->transform.position;
        scene.lights[scene.numLights].direction = light->transform.GetGlobalFront();
        if (!light->shadows || light->shadowType != ShadowType::RayTraced) {
            scene.lights[scene.numLights].numShadowSamples = 0;
        }
        scene.numLights++;
    }
    scene.camPos = camera.GetPosition();
    scene.projView = camera.GetProj() * camera.GetView();
    scene.inverseProj = glm::inverse(camera.GetProj());
    scene.inverseView = glm::inverse(camera.GetView());
    UpdateBuffers(numFrame);
    RayTracing::CreateTLAS(modelEntities);
}

void Scene::DestroyResources() {
    BufferManager::DestroyStorageBuffer(sceneBuffer);
    BufferManager::DestroyStorageBuffer(modelsBuffer);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        DestroyTextureResource(shadowMaps[i]);
    }
}

Model* Scene::CreateModel() {
    Model* model = new Model();
    model->name = "Model";
    model->mesh = 0;
    model->entityType = EntityType::Model;
    entities.push_back(model);
    modelEntities.push_back(model);
    RayTracing::SetRecreateTLAS();
    return model;
}

Model* Scene::CreateModel(Model* copy) {
    Model* entity = new Model();

    entity->name = copy->name;
    entity->transform = copy->transform;
    entity->entityType = EntityType::Model;
    entities.push_back(entity);
    SetParent(entity, copy->parent);

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

Light* Scene::CreateLight() {
    Light* light = new Light();
    light->name = "Light";
    light->entityType = EntityType::Light;
    light->block.type = LightType::Point;
    light->transform.SetScale(glm::vec3(0.1));
    entities.push_back(light);
    lightEntities.push_back(light);
    return light;
}

Light* Scene::CreateLight(Light* copy) {
    Light* entity = new Light();

    entity->name = copy->name;
    entity->transform = copy->transform;
    entity->entityType = EntityType::Light;
    entities.push_back(entity);
    SetParent(entity, copy->parent);

    entity->block = copy->block;
    lightEntities.push_back(entity);
    return entity;
}

Entity* Scene::CreateEntity() {
    Entity* entity = new Entity();
    entities.push_back(entity);
    return entity;
}

Entity* Scene::CreateEntity(Entity* copy) {
    Entity* entity = nullptr;
    if (copy->entityType == EntityType::Model) { 
        entity = CreateModel((Model*)copy); 
    } else if (copy->entityType == EntityType::Light) { 
        entity = CreateLight((Light*)copy); 
    }
    for (int i = 0; i < copy->children.size(); i++) {
        Entity* child = CreateEntity(copy->children[i]);
        SetParent(child, entity);
    }
    DEBUG_ASSERT(entity != nullptr, "Entity type invalid during copy.");
    return entity;
}

void Scene::DeleteEntity(Entity* entity) {
    if (selectedEntity == entity) {
        selectedEntity = nullptr;
    }
    if (copiedEntity == entity) {
        copiedEntity = nullptr;
    }
    if (entity->parent != nullptr) {
        RemoveFromParent(entity);
    }
    if (entity->entityType == EntityType::Model) {
        RayTracing::SetRecreateTLAS();
        auto it = std::find(modelEntities.begin(), modelEntities.end(), (Model*)entity);
        DEBUG_ASSERT(it != modelEntities.end(), "Model isn't on the vector of models.");
        modelEntities.erase(it);
    } else if (entity->entityType == EntityType::Light) {
        auto it = std::find(lightEntities.begin(), lightEntities.end(), (Light*)entity);
        DEBUG_ASSERT(it != lightEntities.end(), "Model isn't on the vector of models.");
        lightEntities.erase(it);
    }
    while (entity->children.size()) {
        DeleteEntity(*entity->children.begin());
    }
    auto it = std::find(entities.begin(), entities.end(), entity);
    DEBUG_ASSERT(it != entities.end(), "Entity isn't on the vector of entities.");
    entities.erase(it);
    delete entity;
}

void Scene::DeleteModel(Model* model) {
    if (model->parent != nullptr) {
        RemoveFromParent(model);
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

void Scene::RemoveFromParent(Entity* entity) {
    Entity* parent = entity->parent;
    if (parent == nullptr) {
        return;
    }
    auto it = std::find(parent->children.begin(), parent->children.end(), entity);
    DEBUG_ASSERT(it != parent->children.end(), "Entity isn't a children of its parent.");
    parent->children.erase(it);
    entity->parent = nullptr;
    entity->transform.parent = nullptr;
}

void Scene::SetParent(Entity* entity, Entity* collection) {
    if (entity->parent != nullptr) {
        RemoveFromParent(entity);
    }
    entity->parent = collection;
    entity->transform.parent = &collection->transform;
    collection->children.push_back(entity);
}

void Scene::OnImgui() {
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
        for (Entity* entity : entities) {
            if (entity->parent == nullptr) {
                OnImgui(entity);
            }
        }
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
    }
    static std::string filename = "assets/scene.gltf";
    if (ImGui::Button("Save") || (controlPressed && Window::IsKeyPressed(GLFW_KEY_S))) {
        AssetManager::SaveGLTF(filename, *this);
    }
    ImGui::SameLine();
    ImGui::InputText("Filename", &filename);
}

void Scene::OnImgui(Entity* ent) {
    ImGui::PushID(ent);
    ImGuiTreeNodeFlags flags = selectedEntity == ent ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    if (ent->children.size() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    bool open = ImGui::TreeNodeEx(ent->name.c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedEntity = ent;
    }
    if (open) {
        for (Entity* child : ent->children) {
            OnImgui(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void Scene::InspectModel(Model* model) {
    ImGui::Text("Mesh: %d", model->mesh);
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNode("Color")) {
            ImGui::ColorEdit3("Color", glm::value_ptr(model->block.material.color));
            ImGui::Checkbox("Use texture", &model->useColorMap);
            DrawTextureOnImgui(AssetManager::textures[model->block.material.colorMap]);
            AcceptTexturePayload(model->block.material.colorMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Normal")) {
            ImGui::Checkbox("Use texture", &model->useNormalMap);
            DrawTextureOnImgui(AssetManager::textures[model->block.material.normalMap]);
            AcceptTexturePayload(model->block.material.normalMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Metallic Roughness")) {
            ImGui::DragFloat("Metallic", &model->block.material.metallic, 0.001, 0, 1);
            ImGui::DragFloat("Roughness", &model->block.material.roughness, 0.001, 0, 1);
            ImGui::Checkbox("Use texture", &model->useMetallicRoughnessMap);
            DrawTextureOnImgui(AssetManager::textures[model->block.material.metallicRoughnessMap]);
            AcceptTexturePayload(model->block.material.metallicRoughnessMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Emission")) {
            ImGui::ColorEdit3("Emission", glm::value_ptr( model->block.material.emission));
            ImGui::Checkbox("Use texture", &model->useEmissionMap);
            DrawTextureOnImgui(AssetManager::textures[model->block.material.emissionMap]);
            AcceptTexturePayload(model->block.material.emissionMap);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Ambient Occlusion")) {
            ImGui::Checkbox("Use texture", &model->useAoMap);
            DrawTextureOnImgui(AssetManager::textures[model->block.material.aoMap]);
            AcceptTexturePayload(model->block.material.aoMap);
            ImGui::TreePop();
        }
    }
}

void Scene::InspectLight(Light* light) {
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
    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Active", &light->shadows);
        ImGui::DragInt("Num samples", (int*) &light->block.numShadowSamples, 1, 1, 64);
        ImGui::DragFloat("Radius", &light->block.radius, 0.01, 0.0f);
        light->block.radius = std::max(light->block.radius, 0.0f);
        DrawTextureOnImgui(shadowMaps[light->id]);
        ImGui::DragFloat3("P0", (float*) & light->p0);
        ImGui::DragFloat3("P1", (float*) & light->p1);
    }
}

void Scene::InspectEntity(Entity* entity) {
    ImGui::InputText("Name", &entity->name);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Reset")) {
            entity->transform = {};
        }
        float speed = 0.01f;
        bool changed = ImGui::DragFloat3("Position", (float*) & entity->transform.position, speed);
        changed |= ImGui::DragFloat3("Scale", (float*) & entity->transform.scale, speed);
        changed |= ImGui::DragFloat3("Rotation", (float*) & entity->transform.rotation, speed);
        if (changed) {
            //entity->transform.scale = glm::max(glm::abs(entity->transform.scale), 0.001f);
            entity->transform.dirty = true;
            entity->transform.globalDirty = true;
        }
        RenderTransformGizmo(entity->transform);
    }
    if (entity->entityType == EntityType::Model) {
        InspectModel((Model*)entity);
    } else if (entity->entityType == EntityType::Light) {
        InspectLight((Light*)entity);
    }
}

void Scene::RenderTransformGizmo(Transform& transform) {
    ImGuizmo::BeginFrame();
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_0)) {
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

void Scene::AcceptMeshPayload() {
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("mesh");
        if (payload) {
            std::string path((const char*)payload->Data, payload->DataSize);
            AssetManager::LoadModel(path, *this);
        }
    }
}

void AcceptTexturePayload(RID& textureID) {
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payloadTexture = ImGui::AcceptDragDropPayload("texture");
        if (payloadTexture) {
            std::string path((const char*)payloadTexture->Data, payloadTexture->DataSize);
            textureID = AssetManager::LoadTexture(path);
        }
        const ImGuiPayload* payloadTextureID = ImGui::AcceptDragDropPayload("textureID");
        if (payloadTextureID) {
            textureID = *(RID*)payloadTextureID->Data;
        }
    }
}
