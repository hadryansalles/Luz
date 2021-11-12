#include "Resources.hpp"

#include <imgui/imgui_stdlib.h>

void CreateScene(SceneResource& res) {
    auto pointDesc = AssetManager::LoadModels("assets/point.obj");
    DEBUG_ASSERT(pointDesc.size() > 0, "Can't load point light gizmo mesh!");
    auto dirDesc = AssetManager::LoadModels("assets/directional.obj");
    DEBUG_ASSERT(pointDesc.size() > 0, "Can't load directional light gizmo mesh!");
    auto spotDesc = AssetManager::LoadModels("assets/spot.obj");
    DEBUG_ASSERT(pointDesc.size() > 0, "Can't load spot light gizmo mesh!");
    res.lightMeshes[0] = pointDesc[0].mesh;
    res.lightMeshes[1] = dirDesc[0].mesh;
    res.lightMeshes[2] = spotDesc[0].mesh;
    AssetManager::AsyncLoadModels("assets/ignore/sponza/sponza_semitransparent.obj");
    res.rootCollection = CreateCollection(res, "Root");
}

void CreateSceneResources(SceneResource& res) {
    BufferManager::CreateStorageBuffer(res.sceneBuffer, sizeof(res.scene));
    BufferManager::CreateStorageBuffer(res.modelsBuffer, sizeof(res.models));
    GraphicsPipelineManager::WriteStorage(res.sceneBuffer, SCENE_BUFFER_INDEX);
    GraphicsPipelineManager::WriteStorage(res.modelsBuffer, MODELS_BUFFER_INDEX);
}

Model* CreateModel(SceneResource& res, std::string name, RID mesh, RID texture) {
    Model* model = new Model();
    model->transform.SetPosition(AssetManager::meshDescs[mesh].center);
    model->name = name;
    model->mesh = mesh;
    model->entityType = EntityType::Model;
    model->block.textures[0] = texture;
    res.entities.push_back(model);
    SetCollection(model, res.rootCollection);
    return model;
}

void InspectEntity(SceneResource& res, Entity* entity) {
    ImGui::InputText("Name", & entity->name);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", glm::value_ptr(entity->transform.position));
        ImGui::DragFloat3("Scale", glm::value_ptr(entity->transform.scale));
        ImGui::DragFloat3("Rotation", glm::value_ptr(entity->transform.rotation));
        RenderTransformGizmos(res, entity->transform);
    }
    if (entity->entityType == EntityType::Model) {
        InspectModel(res, (Model*)entity);
    } else if (entity->entityType == EntityType::Light) {
        InspectLight(res, (Light*)entity);
    }
}
