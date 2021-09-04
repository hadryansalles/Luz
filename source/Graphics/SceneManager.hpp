#pragma once

#include <filesystem>
#include <Model.hpp>
#include "Descriptors.hpp"
#include <thread>

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
    static inline Transform* selectedTransform = nullptr;

    static inline std::vector<ModelDesc> preloadedModels;
    static inline std::mutex preloadedModelsLock;

    static void AddModel(Model* model, Collection* collection = nullptr);
    static void SetCollection(Model* model, Collection* collection = nullptr);

    static void CollectionOnImgui(Collection* collection, int id);
    static void ModelOnImgui(Model* model);

public:
    static void Setup();
    static void Create();
    static void Destroy();
    static void Update();
    static void Finish();
    static void OnImgui();

    static Model* CreateModel(ModelDesc& desc);
    static void SetTexture(Model* model, TextureResource* texture);

    static Collection* CreateCollection(Collection* parent = nullptr);

    static void LoadModels(std::filesystem::path path, Collection* collection = nullptr);
    static void AsyncLoadModels(std::filesystem::path path, Collection* collection = nullptr);
    static void AddPreloadedModel(ModelDesc desc);

    static inline BufferDescriptor& GetSceneDescriptor() { return sceneDescriptor;   }
    static inline std::vector<Model*>& GetModels()       { return models;            }
    static inline Model* GetSelectedModel()              { return selectedModel;     }
    static inline Transform* GetSelectedTransform()      { return selectedTransform; }
};
