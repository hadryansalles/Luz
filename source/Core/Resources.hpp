#pragma once

#include "Transform.hpp"
#include "Camera.hpp"
#include "AssetManager.hpp"
#include "BufferManager.hpp"
#include "GraphicsPipelineManager.hpp"

#define MAX_TEXTURES 4096
#define MAX_MODELS 4096
#define MAX_LIGHTS 16
#define MAX_MESHES 2048

#define SCENE_BUFFER_INDEX 0
#define MODELS_BUFFER_INDEX 1

struct LightBlock {
    glm::vec3 color = glm::vec3(1.0f);
    f32 intensity = 1.0f;
    glm::vec3 position = glm::vec3(1.0f);
    f32 innerAngle = 0;
    glm::vec3 direction = glm::vec3(.0f, .0f, 1.0f);
    f32 outerAngle = 0;
    u32 type = 0;
    u32 PADDING[3];
};

struct SceneBlock {
    LightBlock lights[MAX_LIGHTS];
    glm::vec3 ambientLightColor = glm::vec3(1.0f);
    f32 ambientLightIntensity = 0.1f;
    glm::mat4 projView = glm::mat4(1.0f);
    glm::vec3 camPos = glm::vec3(.0f, .0f, .0f);
    u32 numLights = 0;
};

struct ModelBlock {
    glm::mat4 model     = glm::mat4(1.0f);
    glm::vec4 colors[2] = { glm::vec4(1.0f), glm::vec4(1.0f) };
    f32 values[2]       = { .0f, .0f };
    RID textures[2]     = { 0, 0 };
};

struct ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

enum class EntityType {
    Invalid,
    Model,
    Collection,
    Light
};

struct Entity {
    std::string name = "Entity";
    Transform transform;
    EntityType entityType = EntityType::Invalid;
    struct Collection* parent = nullptr;
};

enum LightType {
    Point,
    Directional,
    Spot
};

enum class Material {
    Phong,
    Unlit
};

struct Model : Entity {
    RID mesh = 0;
    ModelBlock block;
    Material material = Material::Phong;
};

struct Light : Entity {
    LightBlock block;
};

struct Collection : Entity {
    std::vector<Entity*> children;
};

struct SceneResource {
    SceneBlock scene;
    ModelBlock models[MAX_MODELS];
    RID textures[MAX_TEXTURES];
    StorageBuffer sceneBuffer;
    StorageBuffer modelsBuffer;
    std::vector<RID> freeTextureRIDs;
    std::vector<RID> freeModelRIDs; 

    Camera camera;
    Collection* rootCollection = nullptr;
    std::vector<Entity*> entities;
    Entity* selectedEntity = nullptr;

    bool renderLightGizmos = true;
    float lightGizmosOpacity = 1.0f;

    RID lightMeshes[3];
};

void CreateScene(SceneResource& res);

inline void RemoveFromCollection(Entity* entity) {
    Collection* parent = entity->parent;
    auto it = std::find(parent->children.begin(), parent->children.end(), entity);
    DEBUG_ASSERT(it != parent->children.end(), "Entity isn't a children of its parent.");
    parent->children.erase(it);
    entity->parent = nullptr;
    entity->transform.parent = nullptr;
}

inline void SetCollection(Entity* entity, Collection* collection) {
    if (entity->parent != nullptr) {
        RemoveFromCollection(entity);
    }
    entity->parent = collection;
    entity->transform.parent = &collection->transform;
    collection->children.push_back(entity);
}

Model* CreateModel(SceneResource& res, std::string name, RID mesh, RID texture);

inline Light* CreateLight(SceneResource& res, std::string name, LightType type) {
    Light* light = new Light();
    light->name = name;
    light->entityType = EntityType::Light;
    light->block.type = type;
    res.entities.push_back(light);
    return light;
}

inline Collection* CreateCollection(SceneResource& res, std::string name) {
    Collection* collection = new Collection();
    collection->entityType = EntityType::Collection;
    collection->name = name;
    res.entities.push_back(collection);
    return collection;
}

void CreateSceneResources(SceneResource& res);

inline void DestroySceneResources(SceneResource& res) {
    BufferManager::DestroyStorageBuffer(res.sceneBuffer);
    BufferManager::DestroyStorageBuffer(res.modelsBuffer);
}

inline void UpdateSceneResource(SceneResource& res, int numFrame) {
    LUZ_PROFILE_FUNC();
    res.scene.numLights = 0;
    for (int i = 0; i < res.entities.size(); i++) {
        Entity* entity = res.entities[i];
        if (entity->entityType == EntityType::Model) {
            Model* model = (Model*)entity;
            res.models[i] = model->block;
            res.models[i].model = entity->transform.GetMatrix();
        } if (entity->entityType == EntityType::Light) {
            Light* light = (Light*)entity;
            res.models[i].model = entity->transform.GetMatrix();
            res.models[i].colors[0] = glm::vec4(light->block.color, res.lightGizmosOpacity);
            res.models[i].textures[0] = 1;
            res.scene.lights[res.scene.numLights] = light->block;
            res.scene.lights[res.scene.numLights].position = entity->transform.position;
            res.scene.lights[res.scene.numLights].direction = entity->transform.GetGlobalFront();
            res.scene.numLights++;
        }
    }
    res.scene.projView = res.camera.GetProj() * res.camera.GetView();
    BufferManager::UpdateStorage(res.sceneBuffer, numFrame, &res.scene);
    BufferManager::UpdateStorage(res.modelsBuffer, numFrame, &res.models);
}

inline void OnImgui(SceneResource& res, Collection* collection, bool root = false) {
    bool open = true;
    if (!root) {
        ImGui::PushID(collection);
        open = ImGui::TreeNode(collection->name.c_str());
    }
    if (open) {
        for (Entity* entity : collection->children) {
            if (entity->entityType == EntityType::Collection) {
                OnImgui(res, (Collection*)entity);
            }
            else {
                if (ImGui::Selectable(entity->name.c_str(), res.selectedEntity == entity)) {
                    res.selectedEntity = entity;
                }
            }

        }
        if (!root) {
            ImGui::TreePop();
            ImGui::PopID();
        }
    }
}

inline void OnImgui(SceneResource& res) {
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNodeEx("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            OnImgui(res, res.rootCollection, true);
            ImGui::TreePop();
        }
        if (ImGui::Button("Add model")) {
            CreateModel(res, "New Model", 0, 0);
        }
        if (ImGui::Button("Add light")) {
            CreateLight(res, "New Light", LightType::Point);
        }
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(res.scene.ambientLightColor));
        ImGui::DragFloat("Ambient intensity", &res.scene.ambientLightIntensity, 0.01);
    }
}

inline void InspectModel(SceneResource& res, Model* model) {
    ImGui::Text("Mesh: %d", model->mesh);
}

inline void InspectLight(SceneResource& res, Light* light) {
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

inline void RenderTransformGizmos(SceneResource& res, Transform& transform) {
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
    glm::mat4 guizmoProj(res.camera.GetProj());
    guizmoProj[1][1] *= -1;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(res.camera.GetView()), glm::value_ptr(guizmoProj), currentGizmoOperation,
    currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

    if (transform.parent != nullptr) {
        modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
    }
    transform.transform = modelMat;
    
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.transform), glm::value_ptr(transform.position), 
        glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));
}

void InspectEntity(SceneResource& res, Entity* entity);