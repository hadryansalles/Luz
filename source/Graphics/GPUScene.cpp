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
    bool anyVolumetricLight = false;
    bool anyShadowMap = false;

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
    mesh.vertexCount = u32(asset->vertices.size());
    mesh.indexCount = u32(asset->indices.size());
    mesh.vertexBuffer = vkw::CreateBuffer(
        sizeof(MeshAsset::MeshVertex) * mesh.vertexCount,
        vkw::BufferUsage::Vertex | vkw::BufferUsage::AccelerationStructureInput | vkw::BufferUsage::Storage,
        vkw::Memory::GPU,
        ("VertexBuffer#" + std::to_string(asset->uuid))
    );
    mesh.indexBuffer = vkw::CreateBuffer(
        sizeof(uint32_t) * mesh.indexCount,
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
    }

    SceneBlock& s = impl->sceneBlock;
    s.numLights = 0;

    s.camPos = camera->eye;
    s.prevViewProj = s.viewProj;
    s.prevJitter = camera->GetJitter();
    camera->NextJitter();
    s.jitter = camera->GetJitter();
    s.proj = camera->GetProjJittered();
    s.view = camera->GetView();
    s.viewProj = camera->GetProjJittered() * camera->GetView();
    s.inverseProj = glm::inverse(camera->GetProjJittered());
    s.inverseView = glm::inverse(camera->GetView());

    impl->anyShadowMap = scene->shadowType == ShadowType::ShadowMap;
    impl->anyVolumetricLight = false;

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
            impl->anyVolumetricLight = true;
        } else if (light->volumetricType == LightNode::VolumetricType::ShadowMap) {
            block.volumetricWeight = light->volumetricShadowMapParams.weight;
            block.volumetricSamples = light->volumetricShadowMapParams.samples;
            block.volumetricDensity = light->volumetricShadowMapParams.density;
            block.volumetricAbsorption = light->volumetricShadowMapParams.absorption;
            impl->anyVolumetricLight = true;
            impl->anyShadowMap = true;
        }

        bool isDirectional = light->lightType == LightNode::LightType::Directional;
        glm::vec3 pos = light->GetWorldPosition();
        if (!isDirectional) {
            glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.0f, light->shadowMapFar);
            block.viewProj[0] = proj * glm::lookAt(pos, pos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
            block.viewProj[1] = proj * glm::lookAt(pos, pos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
            block.viewProj[2] = proj * glm::lookAt(pos, pos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
            block.viewProj[3] = proj * glm::lookAt(pos, pos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
            block.viewProj[4] = proj * glm::lookAt(pos, pos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
            block.viewProj[5] = proj * glm::lookAt(pos, pos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
        } else {
            // Calculate view frustum corners in world space
            std::vector<glm::vec4> frustumCorners;
            frustumCorners.reserve(8);
            
            // Get camera view frustum with near/far adjusted by shadowMapRange
            float adjustedFar = camera->farDistance / light->shadowMapRange;
            glm::mat4 camProjView = glm::inverse(camera->GetProj(camera->nearDistance, adjustedFar) * s.view);
            
            // Get 8 corners of view frustum
            for (int x = 0; x < 2; x++) {
                for (int y = 0; y < 2; y++) {
                    for (int z = 0; z < 2; z++) {
                        glm::vec4 corner = camProjView * glm::vec4(
                            2.0f * x - 1.0f,  // -1 or 1
                            2.0f * y - 1.0f,  // -1 or 1 
                            2.0f * z - 1.0f,  // -1 or 1
                            1.0f
                        );
                        frustumCorners.push_back(corner / corner.w);
                    }
                }
            }

            // Calculate center of frustum
            glm::vec3 frustumCenter(0.0f);
            for (const auto& corner : frustumCorners) {
                frustumCenter += glm::vec3(corner);
            }
            frustumCenter /= frustumCorners.size();

            // Create light view matrix looking at frustum center
            glm::mat4 lightView = glm::lookAt(
                frustumCenter + light->GetWorldFront(), 
                frustumCenter,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            // Transform frustum corners to light space and find bounds
            glm::vec3 minCorner(std::numeric_limits<float>::max());
            glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

            for (const auto& corner : frustumCorners) {
                glm::vec3 lightSpaceCorner = glm::vec3(lightView * corner);
                minCorner = glm::min(minCorner, lightSpaceCorner);
                maxCorner = glm::max(maxCorner, lightSpaceCorner);
            }

            // Adjust z bounds based on shadowMapRange
            float r = light->shadowMapRange;
            minCorner.z = minCorner.z < 0 ? minCorner.z * r : minCorner.z / r;
            maxCorner.z = maxCorner.z < 0 ? maxCorner.z / r : maxCorner.z * r;

            // Create orthographic projection matrix from bounds
            glm::mat4 lightProj = glm::ortho(
                minCorner.x, maxCorner.x,
                minCorner.y, maxCorner.y, 
                maxCorner.z, minCorner.z
            );

            // Store final light space matrix
            block.viewProj[0] = lightProj * lightView;
        }

        auto it = impl->shadowMaps.find(light->uuid);
        if (it == impl->shadowMaps.end() || (isDirectional == (it->second.img.layers == 6))) {
            if (it != impl->shadowMaps.end()) {
                impl->shadowMaps.erase(it);
            }
            impl->shadowMaps[light->uuid].img = vkw::CreateImage({
                .width = scene->shadowResolution,
                .height = scene->shadowResolution,
                .format = vkw::Format::D32_sfloat,
                .usage = vkw::ImageUsage::DepthAttachment | vkw::ImageUsage::Sampled,
                .name = "ShadowMap" + std::to_string(light->uuid),
                .layers = isDirectional ? 1u : 6u,
                .samplerType = vkw::SamplerType::Nearest,
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
}

void GPUScene::UpdateResourcesGPU() {
    if (impl->modelsBlock.size() == 0) {
        return;
    }
    vkw::CmdCopy(impl->modelsBuffer, impl->modelsBlock.data(), sizeof(ModelBlock) * u32(impl->modelsBlock.size()));
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
        vkw::CmdCopy(impl->linesBuffer, points.data(), sizeof(glm::vec3) * u32(points.size()));
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

bool GPUScene::AnyVolumetricLight() {
    return impl->anyVolumetricLight;
}