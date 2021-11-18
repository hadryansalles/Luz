#pragma once

#include <string>

#include "Transform.hpp"
#include "Camera.hpp"
#include "BufferManager.hpp"

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

struct MaterialBlock {
    glm::vec3 color  = glm::vec3(0.4, 0.4, 0.4);
    float specular   = 0.5;

    float emission   = 0;
    float opacity    = 1;
    float metallic   = 0;
    float roughness  = 0.4;

    RID colorMap     = 0;
    RID normalMap    = 1;
    RID metallicMap  = 1;
    RID roughnessMap = 1;
};

struct ModelBlock {
    glm::mat4 model = glm::mat4(1.0f);
    MaterialBlock material;
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

struct Model : Entity {
    RID id = 0;
    RID mesh = 0;
    ModelBlock block;
    bool useColorMap = true;
    bool useNormalMap = true;
    bool useMetallicMap = true;
    bool useRoughnessMap = true;
};

struct Light : Entity {
    RID id = 0;
    LightBlock block;
};

struct Collection : Entity {
    std::vector<Entity*> children;
};

namespace Scene {
    inline SceneBlock scene;
    inline ModelBlock models[MAX_MODELS];
    inline RID textures[MAX_TEXTURES];
    inline StorageBuffer sceneBuffer;
    inline StorageBuffer modelsBuffer;
    inline std::vector<RID> freeTextureRIDs;
    inline std::vector<RID> freeModelRIDs; 

    inline Camera camera;
    inline Collection* rootCollection = nullptr;
    inline std::vector<Entity*> entities;
    inline std::vector<Model*> modelEntities;
    inline std::vector<Light*> lightEntities;
    inline Entity* selectedEntity = nullptr;

    inline bool renderLightGizmos = true;
    inline float lightGizmosOpacity = 1.0f;

    inline RID lightMeshes[3];

    void Setup();
    void CreateResources();
    void UpdateResources(int numFrame);
    void DestroyResources();

    Model* CreateModel();
    Model* CreateModel(Model& copy);
    Light* CreateLight();
    Collection* CreateCollection();

    void DeleteModel(Model* model);

    void RemoveFromCollection(Entity* entity);
    void SetCollection(Entity* entity, Collection* collection);

    void OnImgui();
    void OnImgui(Collection* collection, bool root = false);

    void InspectModel(Model* model);
    void InspectLight(Light* light);
    void InspectEntity(Entity* entity);

    void RenderTransformGizmo(Transform& transform);
}
