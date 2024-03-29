#include <imgui/imgui.h>
#include "GPUScene.hpp"
#include "LuzCommon.h"

struct GPUSceneImpl {
    vkw::Buffer sceneBuffer;
    vkw::Buffer modelsBuffer;

    vkw::TLAS tlas;

    SceneBlock sceneBlock;
    std::vector<ModelBlock> modelsBlock;

    ModelBlock defaultModelBlock = {
        .modelMat = glm::mat4(1),
        .color = {1, 1, 1, 1},
        .emission = {0, 0, 0},
        .metallic = 0,
        .roughness = 0.5,
        .aoMap = -1,
        .colorMap = -1,
        .normalMap = -1,
        .emissionMap = -1,
        .metallicRoughnessMap = -1
    };

    std::vector<GPUModel> meshModels;

    std::unordered_map<UUID, GPUMesh> meshes;
    std::unordered_map<UUID, GPUTexture> textures;
};

GPUScene::GPUScene() {
    impl = new GPUSceneImpl;
}

GPUScene::~GPUScene() {
    delete impl;
}

void GPUScene::Create() {
    impl->tlas = vkw::CreateTLAS(LUZ_MAX_MODELS, "mainTLAS");
    impl->sceneBuffer = vkw::CreateBuffer(sizeof(SceneBlock), vkw::BufferUsage::Storage | vkw::BufferUsage::TransferDst);
    impl->modelsBuffer = vkw::CreateBuffer(sizeof(ModelBlock) * LUZ_MAX_MODELS, vkw::BufferUsage::Storage | vkw::BufferUsage::TransferDst);
}

void GPUScene::Destroy() {
    impl->tlas = {};
    impl->sceneBuffer = {};
    impl->modelsBuffer = {};
    ClearAssets();
}

void GPUScene::AddMesh(const Ref<MeshAsset>& asset) {
    GPUMesh& mesh = impl->meshes[asset->uuid];
    mesh.vertexCount = asset->vertices.size();
    mesh.indexCount = asset->indices.size();
    mesh.vertexBuffer = vkw::CreateBuffer(
        sizeof(MeshAsset::MeshVertex) * asset->vertices.size(),
        vkw::BufferUsage::Vertex | vkw::BufferUsage::AccelerationStructureInput,
        vkw::Memory::GPU,
        ("VertexBuffer#" + std::to_string(asset->uuid))
    );
    mesh.indexBuffer = vkw::CreateBuffer(
        sizeof(uint32_t) * asset->indices.size(),
        vkw::BufferUsage::Index | vkw::BufferUsage::AccelerationStructureInput,
        vkw::Memory::GPU,
        ("IndexBuffer#" + std::to_string(asset->uuid))
    );
    mesh.blas = vkw::CreateBLAS ({
        .vertexBuffer = mesh.vertexBuffer,
        .indexBuffer = mesh.indexBuffer,
        .vertexCount = mesh.vertexCount,
        .indexCount = mesh.indexCount,
        .vertexStride = sizeof(MeshAsset::MeshVertex),
        .name = "BLAS#" + std::to_string(asset->uuid)
    });;
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdCopy(mesh.vertexBuffer, asset->vertices.data(), mesh.vertexBuffer.size);
    vkw::CmdCopy(mesh.indexBuffer, asset->indices.data(), mesh.indexBuffer.size);
    vkw::CmdBuildBLAS(mesh.blas);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);
}

void GPUScene::AddTexture(const Ref<TextureAsset>& asset) {
    GPUTexture& texture = impl->textures[asset->uuid];
    texture.image = vkw::CreateImage({
        .width = uint32_t(asset->width),
        .height = uint32_t(asset->height),
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
        .name = "Texture " + std::to_string(asset->uuid),
    });
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdBarrier(texture.image, vkw::Layout::TransferDst);
    vkw::CmdCopy(texture.image, asset->data.data(), asset->width * asset->height * 4);
    vkw::CmdBarrier(texture.image, vkw::Layout::ShaderRead);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);
}

void GPUScene::ClearAssets() {
    impl->meshes.clear();
    impl->textures.clear();
    impl->meshModels.clear();
}

void GPUScene::AddAssets(const AssetManager& assets) {
    const auto& meshes = assets.GetAll<MeshAsset>(ObjectType::MeshAsset);
    for (auto& mesh : meshes) {
        if (mesh->gpuDirty) {
            AddMesh(mesh);
            mesh->gpuDirty = false;
        }
    }
    const auto& textures = assets.GetAll<TextureAsset>(ObjectType::TextureAsset);
    for (auto& texture : textures) {
        if (texture->gpuDirty) {
            AddTexture(texture);
            texture->gpuDirty = false;
        }
    }
}

void GPUScene::UpdateResources(Ref<SceneAsset>& scene, Camera& camera) {
    LUZ_PROFILE_NAMED("UpdateResourcesGPU");
    std::vector<Ref<MeshNode>> meshNodes;
    scene->GetAll<MeshNode>(ObjectType::MeshNode, meshNodes);
    impl->modelsBlock.clear();
    impl->meshModels.clear();
    for (const auto& node : meshNodes) {
        impl->meshModels.push_back(GPUModel{
            .mesh = impl->meshes[node->mesh->uuid],
            .modelRID = uint32_t(impl->modelsBlock.size()),
            .node = node,
        });
        ModelBlock& block = impl->modelsBlock.emplace_back();
        Ref<MaterialAsset> material = node->material;
        block = impl->defaultModelBlock;
        if (material) {
            block.color = node->material->color;
            block.emission = node->material->emission;
            block.metallic = node->material->metallic;
            block.roughness = node->material->roughness;
            if (material->colorMap) {
                block.colorMap = impl->textures[material->colorMap->uuid].image.RID();
            }
            if (material->normalMap) {
                block.normalMap = impl->textures[material->normalMap->uuid].image.RID();
            }
            if (material->metallicRoughnessMap) {
                block.metallicRoughnessMap = impl->textures[material->metallicRoughnessMap->uuid].image.RID();
            }
            if (material->emissionMap) {
                block.emissionMap = impl->textures[material->emissionMap->uuid].image.RID();
            }
        }
        block.modelMat = node->GetWorldTransform();
    }
    // todo: scene, lights

    SceneBlock& s = impl->sceneBlock;
    s.numLights = 0;
    for (const auto& light : scene->GetAll<LightNode>(ObjectType::LightNode)) {
        LightBlock& block = s.lights[s.numLights++];
        block.color = light->color;
        block.intensity = light->intensity;
        block.position = light->GetWorldPosition();
        block.innerAngle = 0;
        block.outerAngle = 0;
        block.direction = { 0, 0, 1 };
        block.type = LightNode::LightType::Point;
        block.numShadowSamples = light->shadows ? scene->lightSamples : 0;
        block.radius = light->radius;
    }
    s.ambientLightColor = scene->ambientLightColor;
    s.ambientLightIntensity = scene->ambientLight;
    s.aoMax = scene->aoMax;
    s.aoMin = scene->aoMin;
    s.aoNumSamples = scene->aoSamples > 0 ? scene->aoSamples : 0;
    s.exposure = scene->exposure;
    s.camPos = camera.GetPosition();
    s.projView = camera.GetProj() * camera.GetView();
    s.inverseProj = glm::inverse(camera.GetProj());
    s.inverseView = glm::inverse(camera.GetView());
    s.tlasRid = impl->tlas.RID();
    s.useBlueNoise = false;
    s.whiteTexture = -1;
    s.blackTexture = -1;
}

void GPUScene::UpdateResourcesGPU() {
    if (impl->modelsBlock.size() == 0) {
        return;
    }
    vkw::CmdCopy(impl->modelsBuffer, impl->modelsBlock.data(), sizeof(ModelBlock)*impl->modelsBlock.size());
    vkw::CmdCopy(impl->sceneBuffer, &impl->sceneBlock, sizeof(SceneBlock));
    vkw::CmdTimeStamp("GPUScene::BuildTLAS", [&] {
        std::vector<vkw::BLASInstance> vkwInstances(impl->meshModels.size());
        for (int i = 0; i < impl->meshModels.size(); i++) {
            GPUModel& model = impl->meshModels[i];
            vkwInstances[i] = vkw::BLASInstance{
                .blas = model.mesh.blas,
                .modelMat = model.node->GetWorldTransform(),
                .customIndex = model.modelRID,
            };
        }
        vkw::CmdBuildTLAS(impl->tlas, vkwInstances);
    });
}

std::vector<GPUModel>& GPUScene::GetMeshModels() {
    return impl->meshModels;
}

RID GPUScene::GetSceneBuffer() {
    return impl->sceneBuffer.RID();
}

RID GPUScene::GetModelsBuffer() {
    return impl->modelsBuffer.RID();
}
