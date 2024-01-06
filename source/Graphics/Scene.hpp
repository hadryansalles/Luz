#pragma once

#include <string>

#include "Transform.hpp"
#include "Camera.hpp"
#include "BufferManager.hpp"
#include "Texture.hpp"

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
    u32 numShadowSamples = 1;
    float radius = 1;
    u32 PADDING[1];
};

struct SceneBlock {
    LightBlock lights[MAX_LIGHTS];
    glm::vec3 ambientLightColor = glm::vec3(1.0f);
    f32 ambientLightIntensity = 0.03f;
    glm::mat4 projView = glm::mat4(1.0f);
    glm::mat4 inverseProj = glm::mat4(1.0f);
    glm::mat4 inverseView = glm::mat4(1.0f);
    glm::vec3 camPos = glm::vec3(.0f, .0f, .0f);
    u32 numLights = 0;
    u32 aoNumSamples = 1;
    float aoMin = 0.001;
    float aoMax = 0.1;
    float aoPower = 1.45;
    glm::ivec2 viewSize;
    u32 useBlueNoise = 0;
};

struct MaterialBlock {
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec3 emission = glm::vec3(1.0f);
    f32 metallic    = 1;
    f32 roughness   = 1;
    RID aoMap       = 0;
    RID colorMap    = 0;
    RID normalMap   = 0;
    RID emissionMap = 1;
    RID metallicRoughnessMap = 0;
    u32 PADDING[2];
};

struct ModelBlock {
    glm::mat4 model = glm::mat4(1.0f);
    MaterialBlock material;
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

enum class ShadowType {
    Disabled,
    RayTraced,
    ShadowMap,
};

struct Model : Entity {
    RID id = 0;
    RID mesh = 0;
    ModelBlock block;
    bool useColorMap = true;
    bool useNormalMap = true;
    bool useMetallicRoughnessMap = true;
    bool useEmissionMap = true;
    bool useAoMap = true;
};

struct Light : Entity {
    RID id = 0;
    LightBlock block;
    bool shadows = true;
    ShadowType shadowType = ShadowType::RayTraced;
    glm::vec3 p0 = { -10.0, 10.0, 0.0 };
    glm::vec3 p1 = { 10.0, -10.0, 2000.0 };

    glm::mat4 GetShadowViewProjection() {
        if (block.type == LightType::Directional) {
            glm::mat4 view = glm::lookAt(transform.position, transform.position + transform.GetGlobalFront(), glm::vec3(.0f, 1.0f, .0f));
            glm::mat4 proj = glm::ortho(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z);
            return proj * view;
        } else {
            return glm::mat4(1);
        }
    }
};

struct Collection : Entity {
    std::vector<Entity*> children;
};

struct Scene {
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
    std::vector<Model*> modelEntities;
    std::vector<Light*> lightEntities;
    Entity* selectedEntity = nullptr;
    Entity* copiedEntity = nullptr;

    u32 shadowMapSize = 1024;
    TextureResource shadowMaps[MAX_LIGHTS];

    bool renderLightGizmos = true;
    float lightGizmosOpacity = 1.0f;

    bool aoActive = true;
    int aoNumSamples = 1;

    RID lightMeshes[3];

    void Setup();
    void CreateResources();
    void UpdateResources(int numFrame);
    void UpdateBuffers(int numFrame);
    void DestroyResources();

    Entity* CreateEntity(Entity* copy);

    Model* CreateModel();
    Model* CreateModel(Model* copy);

    Light* CreateLight();
    Light* CreateLight(Light* copy);

    Collection* CreateCollection();
    Collection* CreateCollection(Collection* copy);

    void DeleteCollection(Collection* collection);
    void DeleteEntity(Entity* entity);
    void DeleteModel(Model* mdoel);

    void RemoveFromCollection(Entity* entity);
    void SetCollection(Entity* entity, Collection* collection);

    void OnImgui();
    void OnImgui(Collection* collection, bool root = false);

    void InspectModel(Model* model);
    void InspectLight(Light* light);
    void InspectEntity(Entity* entity);

    void RenderTransformGizmo(Transform& transform);
    void AcceptMeshPayload();

    void Save(const std::string& filename);
    bool Read(const std::string& filename);
};
