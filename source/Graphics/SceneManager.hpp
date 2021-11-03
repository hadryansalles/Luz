#pragma once

#include <filesystem>
#include <thread>

#include "Model.hpp"
#include "Descriptors.hpp"
#include "Light.hpp"

// vulkan always expect data to be alligned with multiples of 16
struct SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
};

class SceneManager {
private:
    static inline std::filesystem::path path = "assets/Default.Luz";
    static inline bool dirty = true;
    static inline bool autoReloadFiles = false;

    static inline SceneUBO sceneUBO;
    static inline BufferDescriptor sceneDescriptor;

    static inline unsigned int modelID = 0;
    static inline std::vector<Model*> models;
    static inline Collection mainCollection;
    static inline std::vector<Collection*> collections;

    static inline Model* selectedModel = nullptr;
    static inline Light* selectedLight = nullptr;
    static inline Transform* selectedTransform = nullptr;
    static inline Collection* selectedCollection = nullptr;
    static inline Model* copiedModel = nullptr;
    static inline Light* copiedLight = nullptr;
    static inline Collection* copiedCollection = nullptr;

    static inline bool openSceneItemMenu = false;

    static inline std::vector<ModelDesc> preloadedModels;
    static inline std::mutex preloadedModelsLock;

    static void AddModel(Model* model, Collection* collection = nullptr);
    static void AddLight(Light* light, Collection* collection = nullptr);

    static void SetCollection(Model* model, Collection* collection = nullptr);
    static void SetCollection(Light* model, Collection* collection = nullptr);
    static void SetCollectionParent(Collection* child, Collection* parent);
    static bool CollectionCanBeChild(Collection* child, Collection* parent);

    static void CollectionOnImgui(Collection* collection, int id);
    static void ModelOnImgui(Model* model);
    static void LightOnImgui(Light* light);

    static void SelectCollection(Collection* collection);
    static void SelectModel(Model* model);
    static void SelectLight(Light* light);

    static void SetCopiedCollection(Collection* collection);
    static void SetCopiedModel(Model* model);
    static void SetCopiedLight(Light* light);

    static void CollectionDragDropTarget(Collection* collection);

public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Update();
    static void Finish();
    static void OnImgui();

    static Model* CreateModel(ModelDesc& desc);
    static Model* CreateGizmoModel(MeshResource* mesh);
    static Model* CreateModelCopy(const Model* copy);
    static Model* AddModelCopy(const Model* copy);

    static void DestroyModel(Model* model);
    static void DeleteModelFromCollection(Model* model);
    static void DeleteModel(Model* model);
    static void DeleteLightFromCollection(Light* light);
    static void DeleteLight(Light* light);

    static void AsyncLoadAndSetTexture(Model* model, std::filesystem::path path);
    static void LoadAndSetTexture(Model* model, std::filesystem::path path);

    static Collection* CreateCollection(Collection* parent = nullptr);
    static Collection* CreateCollectionCopy(Collection* copy, Collection* parent = nullptr);
    static void DeleteCollection(Collection* collection);
    static void RemoveCollectionFromParent(Collection* collection);

    static void LoadModels(std::filesystem::path path, Collection* collection = nullptr);
    static void AsyncLoadModels(std::filesystem::path path, Collection* collection = nullptr);
    static void AddPreloadedModel(ModelDesc desc);

    static inline BufferDescriptor& GetSceneDescriptor() { return sceneDescriptor;    }
    static inline std::vector<Model*>& GetModels()       { return models;             }
    static inline Model* GetSelectedModel()              { return selectedModel;      }
    static inline Transform* GetSelectedTransform()      { return selectedTransform;  }
    static inline Collection* GetSelectedCollection()    { return selectedCollection; }
    static inline Light* GetSelectedLight()              { return selectedLight;      }
    static inline std::filesystem::path GetPath()        { return path;               }
};
