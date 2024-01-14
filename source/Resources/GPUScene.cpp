#include "Luzpch.hpp"

#include "GPUScene.hpp"
#include "Managers.hpp"
#include "Shared.hpp"
#include "GraphicsPipelineManager.hpp"
#include "SwapChain.hpp"
#include "RayTracing.hpp"

#define MAX_MODELS 4096

void GPUScene::SetScene(Ref<SceneAsset>& scene) {
    sceneAsset = scene;
    meshNodes.clear();
    for (auto& node : sceneAsset->nodes) {
        node->GetAll<MeshNode>(ObjectType::MeshNode, meshNodes);
    }
}

void GPUScene::SetCamera(Ref<Camera>& camera) {
    this->camera = camera;
}

void GPUScene::CreateResources() {
    BufferManager::CreateStorageBuffer(sceneBuffer, sizeof(SceneBlock2));
    BufferManager::CreateStorageBuffer(modelsBuffer, sizeof(ModelBlock2) * MAX_MODELS);
    GraphicsPipelineManager::WriteStorage(sceneBuffer, SCENE_BUFFER_INDEX);
    GraphicsPipelineManager::WriteStorage(modelsBuffer, MODELS_BUFFER_INDEX);
}

void GPUScene::UpdateResources(int numFrame) {
    LUZ_PROFILE_FUNC();
    sceneAsset->UpdateTransforms();
    std::vector<ModelBlock2> models(meshNodes.size());
    SceneBlock2 scene;
    scene.ambientLightColor = glm::vec3(1, 1, 1);
    scene.ambientLightIntensity = 15.0f;
    scene.numLights = 0;
    scene.viewSize = glm::vec2(SwapChain::GetExtent().width, SwapChain::GetExtent().height);
    for (int i = 0; i < meshNodes.size(); i++) {
        Ref<MeshNode> model = meshNodes[i];
        ModelBlock2 block;
        block.material.color = glm::vec4(1, 0, 0, 1);
        block.model = model->worldTransform;
    }
    scene.camPos = camera->GetPosition();
    scene.projView = camera->GetProj() * camera->GetView();
    scene.inverseProj = glm::inverse(camera->GetProj());
    scene.inverseView = glm::inverse(camera->GetView());
    // RayTracing::CreateTLAS(modelEntities);
    BufferManager::UpdateStorage(sceneBuffer, numFrame, &scene);
    BufferManager::UpdateStorage(modelsBuffer, numFrame, &models);
}

void GPUScene::DestroyResources() {
    BufferManager::DestroyStorageBuffer(sceneBuffer);
    BufferManager::DestroyStorageBuffer(modelsBuffer);
}