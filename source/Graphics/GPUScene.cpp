#include <imgui/imgui.h>
#include "GPUScene.hpp"
#include "LuzCommon.h"
#include "AssetIO.hpp"
#include "DebugDraw.h"

struct GPUSceneImpl {
    vkw::Buffer sceneBuffer;
    vkw::Buffer modelsBuffer;
    vkw::Buffer linesBuffer;

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
        .metallicRoughnessMap = -1,
    };

    std::vector<GPUModel> meshModels;
    std::unordered_map<UUID, ShadowMapData> shadowMaps;

    std::unordered_map<UUID, GPUMesh> meshes;
    std::unordered_map<UUID, GPUTexture> textures;

    vkw::Image blueNoise;
    vkw::Image font;
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
    impl->linesBuffer = vkw::CreateBuffer(sizeof(LineBlock) * LUZ_MAX_LINES, vkw::BufferUsage::Vertex | vkw::BufferUsage::TransferDst);

    // create blue noise resource
    {
        i32 w, h;
        std::vector<u8> blueNoiseData;
        AssetIO::ReadTexture("assets/blue_noise.png", blueNoiseData, w, h);
        impl->blueNoise = vkw::CreateImage({
            .width = uint32_t(w),
            .height = uint32_t(h),
            .format = vkw::Format::RGBA8_unorm,
            .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
            .name = "Blue Noise Texture",
        });
        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        vkw::CmdBarrier(impl->blueNoise, vkw::Layout::TransferDst);
        vkw::CmdCopy(impl->blueNoise, blueNoiseData.data(), w * h * 4);
        vkw::CmdBarrier(impl->blueNoise, vkw::Layout::ShaderRead);
        vkw::EndCommandBuffer();
        vkw::WaitQueue(vkw::Queue::Graphics);
    }
    // create font bitmap
    {
        i32 w, h;
        std::vector<u8> bitmapData;
        AssetIO::ReadTexture("assets/font_bitmap.png", bitmapData, w, h);
        impl->font = vkw::CreateImage({
            .width = uint32_t(w),
            .height = uint32_t(h),
            .format = vkw::Format::RGBA8_unorm,
            .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
            .name = "Font Bitmap Texture",
        });
        vkw::BeginCommandBuffer(vkw::Queue::Graphics);
        vkw::CmdBarrier(impl->font, vkw::Layout::TransferDst);
        vkw::CmdCopy(impl->font, bitmapData.data(), w * h * 4);
        vkw::CmdBarrier(impl->font, vkw::Layout::ShaderRead);
        vkw::EndCommandBuffer();
        vkw::WaitQueue(vkw::Queue::Graphics);
    }
}

void GPUScene::Destroy() {
    impl->tlas = {};
    impl->sceneBuffer = {};
    impl->modelsBuffer = {};
    impl->linesBuffer = {};
    impl->shadowMaps = {};
    impl->blueNoise = {};
    impl->font = {};
    ClearAssets();
    impl = {};
}

void GPUScene::AddMesh(const Ref<MeshAsset>& asset) {
    GPUMesh& mesh = impl->meshes[asset->uuid];
    mesh.vertexCount = asset->vertices.size();
    mesh.indexCount = asset->indices.size();
    mesh.vertexBuffer = vkw::CreateBuffer(
        sizeof(MeshAsset::MeshVertex) * asset->vertices.size(),
        vkw::BufferUsage::Vertex | vkw::BufferUsage::AccelerationStructureInput | vkw::BufferUsage::Storage,
        vkw::Memory::GPU,
        ("VertexBuffer#" + std::to_string(asset->uuid))
    );
    mesh.indexBuffer = vkw::CreateBuffer(
        sizeof(uint32_t) * asset->indices.size(),
        vkw::BufferUsage::Index | vkw::BufferUsage::AccelerationStructureInput | vkw::BufferUsage::Storage,
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
    ASSERT(asset->channels == 4, "Invalid number of channels");
    texture.image = vkw::CreateImage({
        .width = uint32_t(asset->width),
        .height = uint32_t(asset->height),
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
        .name = "Texture " + std::to_string(asset->uuid),
    });
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdBarrier(texture.image, vkw::Layout::TransferDst);
    vkw::CmdCopy(texture.image, asset->data.data(), asset->width * asset->height * asset->channels);
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

void GPUScene::UpdateResources(const Ref<SceneAsset>& scene, const Ref<CameraNode>& camera) {
    LUZ_PROFILE_NAMED("UpdateResources");
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
        block.vertexBuffer = impl->meshes[node->mesh->uuid].vertexBuffer.RID();
        block.indexBuffer = impl->meshes[node->mesh->uuid].indexBuffer.RID();
        block.modelMat = node->GetWorldTransform();

        auto pos = node->GetWorldPosition();
        auto size = node->scale;
#if LUZ_DEBUG
        DebugDraw::Config("model" + std::to_string(node->uuid), { .color = {0.3f, 0.8f, 0.2f, 1.0f}, .update = true});
        DebugDraw::Box("model" + std::to_string(node->uuid),  pos - size * 0.5f, pos + size * 0.5f);
#endif
    }

    SceneBlock& s = impl->sceneBlock;
    s.numLights = 0;

    s.camPos = camera->eye;
    s.proj = camera->GetProj();
    s.view = camera->GetView();
    s.projView = camera->GetProj() * camera->GetView();
    s.inverseProj = glm::inverse(camera->GetProj());
    s.inverseView = glm::inverse(camera->GetView());


    for (const auto& light : scene->GetAll<LightNode>(ObjectType::LightNode)) {
        LightBlock& block = s.lights[s.numLights++];
        block.color = light->color;
        block.intensity = light->intensity;
        block.position = light->GetWorldPosition();
        block.innerAngle = glm::radians(light->innerAngle);
        block.direction = light->GetWorldTransform() * glm::vec4(0, -1, 0, 0);
        block.outerAngle = glm::radians(light->outerAngle);
        block.type = light->lightType;
        block.numShadowSamples = scene->shadowType == ShadowType::ShadowRayTraced ? scene->lightSamples : 0;
        block.radius = light->radius;
        block.zFar = light->shadowMapFar;
        block.shadowMap = -1;
        block.volumetricType = light->volumetricType;
        if (light->volumetricType == LightNode::VolumetricType::ScreenSpace) {
            block.volumetricSamples = light->volumetricScreenSpaceParams.samples;
            block.volumetricAbsorption = light->volumetricScreenSpaceParams.absorption;
        }
        else if (light->volumetricType == LightNode::VolumetricType::ShadowMap) {
            block.volumetricWeight = light->volumetricShadowMapParams.weight;
            block.volumetricSamples = light->volumetricShadowMapParams.samples;
            block.volumetricDensity = light->volumetricShadowMapParams.density;
            block.volumetricAbsorption = light->volumetricShadowMapParams.absorption;
        }

        bool isPoint = false;
        glm::vec3 pos = light->GetWorldPosition();
        if (light->lightType == LightNode::LightType::Point) {
            isPoint = true;
            glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.0f, light->shadowMapFar);
            block.viewProj[0] = proj * glm::lookAt(pos, pos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
            block.viewProj[1] = proj * glm::lookAt(pos, pos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
            block.viewProj[2] = proj * glm::lookAt(pos, pos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
            block.viewProj[3] = proj * glm::lookAt(pos, pos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
            block.viewProj[4] = proj * glm::lookAt(pos, pos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
            block.viewProj[5] = proj * glm::lookAt(pos, pos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
#if LUZ_DEBUG
            DebugDraw::Config("light" + std::to_string(light->uuid), {.update = true});
            DebugDraw::Box("light" + std::to_string(light->uuid), pos - glm::vec3(light->radius), pos + glm::vec3(light->radius));
#endif
        }
        else {
            std::vector<glm::vec4> frustumCorners;
            auto camProjView = glm::inverse(camera->GetProj(camera->nearDistance, camera->farDistance / light->shadowMapRange) * s.view);
            frustumCorners.reserve(8);
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        const glm::vec4 pt = camProjView * glm::vec4(
                            2.0f * i - 1.0f,
                            2.0f * j - 1.0f,
                            2.0f * k - 1.0f,
                            1.0f
                        );
                        frustumCorners.push_back(pt / pt.w);
                    }
                }
            }
            glm::vec3 frustumCenter(0.0f);
            for (const glm::vec4& p : frustumCorners) {
                frustumCenter += glm::vec3(p);
            }
            frustumCenter /= float(frustumCorners.size());

            float r = light->shadowMapRange;
            glm::mat4 view = glm::lookAt(frustumCenter + light->GetWorldFront(), frustumCenter, glm::vec3(.0f, 1.0f, .0f));
            glm::vec3 frustumMin = frustumCorners[0];
            glm::vec3 frustumMax = frustumCorners[0];
            for (const glm::vec4& p : frustumCorners) {
                frustumMin = glm::min(glm::vec3(view * p), frustumMin);
                frustumMax = glm::max(glm::vec3(view * p), frustumMax);
            }
            frustumMin.z = frustumMin.z < 0 ? frustumMin.z * r : frustumMin.z / r;
            frustumMax.z = frustumMax.z < 0 ? frustumMax.z / r : frustumMax.z * r;
            glm::mat4 proj = glm::ortho(frustumMin.x, frustumMax.x, frustumMin.y, frustumMax.y, frustumMax.z, frustumMin.z);
            block.viewProj[0] = proj * view;
        }

        auto it = impl->shadowMaps.find(light->uuid);
        if (it == impl->shadowMaps.end() || (isPoint && it->second.img.layers != 6)) {
            // todo: add shadow map settings to scene and recreate if changed
            impl->shadowMaps[light->uuid].img = vkw::CreateImage({
                .width = scene->shadowResolution,
                .height = scene->shadowResolution,
                .format = vkw::Format::D32_sfloat,
                .usage = vkw::ImageUsage::DepthAttachment | vkw::ImageUsage::Sampled,
                .name = "ShadowMap" + std::to_string(light->uuid),
                .layers = light->lightType == LightNode::LightType::Point ? 6u : 1u,
                });
        }

        impl->shadowMaps[light->uuid].lightIndex = s.numLights - 1;
        block.shadowMap = impl->shadowMaps[light->uuid].img.RID();
    }
    s.ambientLightColor = scene->ambientLightColor;
    s.ambientLightIntensity = scene->ambientLight;
    s.aoMax = scene->aoMax;
    s.aoMin = scene->aoMin;
    s.aoNumSamples = scene->aoSamples > 0 ? scene->aoSamples : 0;
    s.exposure = scene->exposure;
    s.tlasRid = impl->tlas.RID();
    s.blueNoiseTexture = impl->blueNoise.RID();
    s.whiteTexture = -1;
    s.blackTexture = -1;
    s.shadowType = scene->shadowType;


    glm::vec3 starting = { 3, 0.4, 0 };
    glm::vec3 offset = { 2, 0, 0 };

#if LUZ_DEBUG
    DebugDraw::Config("debug_box", {.color = {1.0f, 0.0f, 0.0f, 1.0f}, .update = true});
    DebugDraw::Box("debug_box", starting, starting + glm::vec3(1.0f));

    DebugDraw::Config("debug_rect", {.color = {0.0f, 1.0f, 0.0f, 1.0f}, .update = true});
    DebugDraw::Rect("debug_rect", starting + offset + glm::vec3(0.0f, 0.0f, 0.5f), starting + offset + glm::vec3(0, 1, 0) + glm::vec3(0.0f, 0.0f, 0.5f), starting + offset + glm::vec3(1, 1, 0) + glm::vec3(0.0f, 0.0f, 0.5f), starting + offset + glm::vec3(1, 0, 0) + glm::vec3(0.0f, 0.0f, 0.5f));

    DebugDraw::Config("debug_sphere", {.color = {0.0f, 0.0f, 1.0f, 1.0f}, .update = true});
    DebugDraw::Sphere("debug_sphere", starting + offset * 2.0f + glm::vec3(0.5f), 0.5f);

    DebugDraw::Config("debug_cylinder", {.color = {1.0f, 1.0f, 0.0f, 1.0f}, .update = true});
    DebugDraw::Cylinder("debug_cylinder", starting + offset * 3.0f + glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0.5f);

    DebugDraw::Config("debug_cone", {.color = {1.0f, 0.0f, 1.0f, 1.0f}, .update = true});
    DebugDraw::Cone("debug_cone", starting + offset * 4.0f + glm::vec3(0.5f, 0.0f, 0.5f), starting + offset * 4.0f + glm::vec3(0.5f, 1.0f, 0.5f), 0.5f);
#endif
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

void GPUScene::UpdateLineResources() {
    auto points = DebugDraw::GetPoints();
    if (points.size() > 0) {
        vkw::CmdCopy(impl->linesBuffer, points.data(), sizeof(glm::vec3)*points.size());
    }
}

ShadowMapData& GPUScene::GetShadowMap(UUID uuid) {
    static ShadowMapData invalidData;
    if (impl->shadowMaps.find(uuid) != impl->shadowMaps.end()) {
        return impl->shadowMaps[uuid];
    }
    return invalidData;
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

RID GPUScene::GetFontBitmap() {
    return impl->font.RID();
}

vkw::Buffer GPUScene::GetLinesBuffer() {
    return impl->linesBuffer;
}
