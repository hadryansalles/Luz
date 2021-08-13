#pragma once

#include <filesystem>
#include <Model.hpp>

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
    static inline std::vector<BufferResource> sceneBuffers;
    static inline std::vector<VkDescriptorSet> sceneDescriptors;

    static inline std::vector<Model*> models;
    static inline Model* selectedModel = nullptr;

public:
    static void Setup();
    static void Create();
    static void RecreateDescriptors();
    static void Save();
    static void Destroy();
    static void OnImgui();
    static Model* CreateModel();
    static void SetTexture(Model* model, TextureResource* texture);

    static inline void AddModel(Model* model)                              { models.push_back(model); }
    static inline BufferResource& GetUniformBuffer(uint32_t frameIndex)    { return sceneBuffers[frameIndex];     }
    static inline VkDescriptorSet& GetSceneDescriptor(uint32_t frameIndex) { return sceneDescriptors[frameIndex]; }
    static inline std::vector<Model*>& GetModels()                         { return models;                       }
    static inline Model* GetSelectedModel()                                { return selectedModel;                }
};
