#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "Serializer.hpp"
#include "AssetIO.hpp"
#include "Util.hpp"

#include <imgui/imgui.h>
#include <random>
#include <utility>

Object::~Object()
{}

Asset::~Asset()
{}

TextureAsset::TextureAsset() {
    type = ObjectType::TextureAsset;
}

MeshAsset::MeshAsset() {
    type = ObjectType::MeshAsset;
}

MaterialAsset::MaterialAsset() {
    type = ObjectType::MaterialAsset;
}

SceneAsset::SceneAsset() {
    type = ObjectType::SceneAsset;
}

Node::Node() {
    type = ObjectType::Node;
}

glm::mat4 Node::ComposeTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl, glm::mat4 parent) {
    glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rot)));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 scaleMat = glm::scale(scl);
    return parent * (translationMat * rotationMat * scaleMat);
}

glm::mat4 Node::GetLocalTransform() {
    return ComposeTransform(position, rotation, scale);
}

glm::mat4 Node::GetWorldTransform() {
    return GetParentTransform() * GetLocalTransform();
}

glm::mat4 Node::GetParentTransform() {
    return parent ? parent->GetWorldTransform() : glm::mat4(1);
}

glm::vec3 Node::GetWorldPosition() {
    return GetParentTransform() * glm::vec4(position, 1);
}

glm::vec3 Node::GetWorldFront() {
    return GetWorldTransform() * glm::vec4(0, -1, 0, 0);
}

MeshNode::MeshNode() {
    type = ObjectType::MeshNode;
}

LightNode::LightNode() {
    type = ObjectType::LightNode;
}

void TextureAsset::Serialize(Serializer& s) {
    s.Vector("data", data);
    s("width", width);
    s("height", height);
    s("channels", channels);
}

void MeshAsset::Serialize(Serializer& s) {
    s.Vector("vertices", vertices);
    s.Vector("indices", indices);
}

void MaterialAsset::Serialize(Serializer& s) {
    s("color", color);
    s("emission", emission);
    s("metallic", metallic);
    s("roughness", roughness);
    s.Asset("colorMap", colorMap);
    s.Asset("aoMap", aoMap);
    s.Asset("emissionMap", emissionMap);
    s.Asset("normalMap", normalMap);
    s.Asset("metallicRoughnessMap", metallicRoughnessMap);
}

void SceneAsset::Serialize(Serializer& s) {
    s.VectorRef("nodes", nodes);
    s("ambientLight", ambientLight);
    s("ambientLightColor", ambientLightColor);
    s("lightSamples", lightSamples);
    s("aoSamples", aoSamples);
    s("aoMin", aoMin);
    s("aoMax", aoMax);
    s("exposure", exposure);
    s("shadowType", shadowType);
    s("taaEnabled", taaEnabled);
    s("taaReconstruct", taaReconstruct);
    s.Node("mainCamera", mainCamera, this);
}

void Node::Serialize(Serializer& s) {
    s.VectorRef("children", children);
    s("position", position);
    s("rotation", rotation);
    s("scale", scale);
}

void MeshNode::Serialize(Serializer& s) {
    Node::Serialize(s);
    s.Asset("mesh", mesh);
    s.Asset("material", material);
}

void LightNode::Serialize(Serializer& s) {
    Node::Serialize(s);
    s("color", color);
    s("intensity", intensity);
    s("lightType", lightType);
    s("innerAngle", innerAngle);
    s("outerAngle", outerAngle);
    s("radius", radius);
    s("shadowMapRange", shadowMapRange);
    s("shadowMapFar", shadowMapFar);
    s("volumetricType", volumetricType);

    s("volumetricScreenAbsorption", volumetricScreenSpaceParams.absorption);
    s("volumetricScreenSamples", volumetricScreenSpaceParams.samples);

    s("volumetricShadowWeight", volumetricShadowMapParams.weight);
    s("volumetricShadowAbsorption", volumetricShadowMapParams.absorption);
    s("volumetricShadowDensity", volumetricShadowMapParams.density);
    s("volumetricShadowSamples", volumetricShadowMapParams.samples);
}

// Asset Manager

struct AssetManagerImpl {
    u32 lastAssetsHash = 0;
    Json lastJson;
    std::filesystem::path currentProjectPath;
    std::filesystem::path currentBinPath;
    std::filesystem::path requestedProjectPath;
    std::filesystem::path requestedBinPath;
};

AssetManager::AssetManager() {
    impl = new AssetManagerImpl;
}

AssetManager::~AssetManager() {
    delete impl;
}

Ref<SceneAsset> AssetManager::GetInitialScene() {
    if (!initialScene) {
        CreateAsset<SceneAsset>("DefaultScene");
    }
    return Get<SceneAsset>(initialScene);
}

Ref<CameraNode> AssetManager::GetMainCamera(Ref<SceneAsset>& scene) {
    if (!scene->mainCamera) {
        auto cam = CreateObject<CameraNode>("Default Camera");
        scene->Add(cam);
        scene->mainCamera = cam;
        return cam;
    }
    return scene->mainCamera;
}

void AssetManager::LoadProject(const std::filesystem::path& path, const std::filesystem::path& binPath) {
    TimeScope t("AssetManager::LoadProject", true);
    if (!std::ifstream(path)) {
        Log::Error("Project file not found: {} {}", path.string(), binPath.string());
        return;
    }
    Json j;
    BinaryStorage storage;
    int dir = Serializer::LOAD;
    j = Json::parse(AssetIO::ReadFile(path));
    storage.data = AssetIO::ReadFileBytes(binPath);
    std::vector<UUID> uuids;
    for (auto& assetJson : j["assets"]) {
        Ref<Asset> asset;
        Serializer s = Serializer(assetJson, storage, dir, *this);
        s.Serialize(asset);
        uuids.push_back(asset->uuid);
    }
    for (auto& assetJson : j["scenes"]) {
        Ref<Asset> asset;
        Serializer s = Serializer(assetJson, storage, dir, *this);
        s.Serialize(asset);
        uuids.push_back(asset->uuid);
    }
    initialScene = j["initialScene"];
    for (auto& scene : GetAll<SceneAsset>(ObjectType::SceneAsset)) {
        scene->UpdateParents();
    }
    impl->lastJson = std::move(j);
    impl->lastAssetsHash = HashUUID(uuids);
    impl->currentProjectPath = path;
    impl->currentBinPath = binPath;
}

void AssetManager::SaveProject(const std::filesystem::path& path, const std::filesystem::path& binPath) {
    TimeScope t("AssetManager::SaveProject", true);
    BinaryStorage storage;
    int dir = Serializer::SAVE;
    std::vector<Ref<Asset>> assetsOrdered;
    std::vector<UUID> assetsUUIDs;
    assetsOrdered.reserve(assets.size());
    for (auto& assetPair : assets) {
        assetsOrdered.push_back(assetPair.second);
        assetsUUIDs.push_back(assetPair.first);
    }
    u32 assetsHash = HashUUID(assetsUUIDs);
    if (assetsHash != impl->lastAssetsHash) {
        Log::Info("Serializing assets..");
        Json j;
        j["scenes"] = Json::object();
        // serialize assets
        std::sort(assetsOrdered.begin(), assetsOrdered.end(), [&](const Ref<Asset>& a, const Ref<Asset>& b) {
            return a->type < b->type;
            });
        for (Ref<Asset> asset : assetsOrdered) {
            if (asset->type == ObjectType::SceneAsset) {
                continue;
            }
            Json assetJson;
            Serializer s = Serializer(assetJson, storage, dir, *this);
            s.Serialize(asset);
            j["assets"].push_back(assetJson);
        }
        AssetIO::WriteFileBytes(binPath, storage.data);
        impl->lastJson = std::move(j);
    }
    // always serialize scenes
    for (auto& scene : GetAll<SceneAsset>(ObjectType::SceneAsset)) {
        Json& assetJson = impl->lastJson["scenes"][std::to_string(scene->uuid)];
        Serializer s = Serializer(assetJson, storage, dir, *this);
        s.Serialize(scene);
    }
    impl->lastJson["initialScene"] = initialScene;
    AssetIO::WriteFile(path, impl->lastJson.dump());
    impl->lastAssetsHash = assetsHash;
}

void AssetManager::OnImgui() {
     for (auto& assetPair : assets) {
         auto& asset = assetPair.second;
         if (ImGui::TreeNode(&asset->uuid, "%s", asset->name.c_str())) {
             ImGui::TreePop();
         }
     }
}

UUID AssetManager::NewUUID() {
    // todo: replace with something actually UUID
    static std::random_device rd;
    static std::mt19937_64 eng(rd());
    static std::uniform_int_distribution<u64> dist(std::llround(std::pow(2,61)), std::llround(std::pow(2,62)));
    return dist(eng);
}

std::vector<Ref<Node>> AssetManager::AddAssetsToScene(Ref<SceneAsset>& scene, const std::vector<std::string>& paths) {
    LUZ_PROFILE_NAMED("AddAssetsToScene");
    std::vector<Ref<Node>> newNodes;
    for (const auto& path : paths) {
        UUID uuid = AssetIO::Import(path, *this);
        if (uuid != 0 && assets[uuid]->type == ObjectType::SceneAsset) {
            auto sceneAsset = Get<SceneAsset>(uuid);
            for (auto& node : sceneAsset->nodes) {
                Ref<Node> nodeClone = Node::Clone(node);
                scene->nodes.push_back(nodeClone);
                newNodes.push_back(nodeClone);
            }
        }
    }
    return newNodes;
}

void SceneAsset::DeleteRecursive(const Ref<Node>& node) {
    auto it = std::find_if(nodes.begin(), nodes.end(), [&](auto& child) {
        return child->uuid == node->uuid;
    });
    if (it != nodes.end()) {
        nodes.erase(it);
    }
}

Ref<Node> Node::Clone(Ref<Node>& node) {
    Ref<Object> cloneObject = AssetManager::CloneObject(node->type, std::dynamic_pointer_cast<Object>(node));
    Ref<Node> clone = std::dynamic_pointer_cast<Node>(cloneObject);
    clone->parent = {};
    clone->children.clear();
    for (auto& child : node->children) {
        Ref<Node> childClone = Node::Clone(child);
        SetParent(childClone, clone);
    }
    return clone;
}

// CameraNode
CameraNode::CameraNode() {
    type = ObjectType::CameraNode;
}

void CameraNode::Serialize(Serializer& s) {
    Node::Serialize(s);
    s("cameraType", cameraType);
    s("mode", mode);
    s("eye", eye);
    s("center", center);
    s("rotation", rotation);
    s("zoom", zoom);
    s("farDistance", farDistance);
    s("nearDistance", nearDistance);
    s("horizontalFov", horizontalFov);
    s("orthoFarDistance", orthoFarDistance);
    s("orthoNearDistance", orthoNearDistance);
}

glm::mat4 CameraNode::GetView() {
    glm::vec3 rads;
    if (mode == CameraMode::Orbit) {
        rads = glm::radians(rotation + glm::vec3(90.0f, 90.0f, 0.0f));
        glm::vec3 viewDir;
        viewDir.x = std::cos(-rads.y) * std::sin(rads.x);
        viewDir.z = std::sin(-rads.y) * std::sin(rads.x);
        viewDir.y = std::cos(rads.x);
        if (cameraType == CameraType::Perspective) {
            viewDir *= zoom;
        }
        eye = center - viewDir;
        return glm::lookAt(eye, center, glm::vec3(.0f, 1.0f, .0f));
    } else {
        rads = glm::radians(rotation + glm::vec3(.0f, 180.0f, .0f));
        glm::mat4 rot(1.0f);
        rot = glm::rotate(rot, rads.z, glm::vec3(.0f, .0f, 1.0f));
        rot = glm::rotate(rot, rads.y, glm::vec3(.0f, 1.0f, .0f));
        rot = glm::rotate(rot, rads.x, glm::vec3(1.0f, .0f, .0f));
        return glm::lookAt(eye, eye + glm::vec3(rot[2]), glm::vec3(.0f, 1.0f, .0f));
    }
}

glm::mat4 CameraNode::GetProjJittered() {
    return glm::translate(glm::vec3(jitter.x, jitter.y, 0)) * GetProj();
}

glm::mat4 CameraNode::GetProj() {
    return GetProj(nearDistance, farDistance);
}

glm::mat4 CameraNode::GetProj(float zNear, float zFar) {
    glm::mat4 proj;
    if (cameraType == CameraType::Perspective) {
        proj = glm::perspective(glm::radians(horizontalFov), extent.x / extent.y, zNear, zFar);
    } else {
        glm::vec2 size = glm::vec2(1.0, extent.y / extent.x) * zoom;
        proj = glm::ortho(-size.x, size.x, -size.y, size.y, orthoNearDistance, orthoFarDistance);
    }
    // glm was designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    // the easiest way to fix this is fliping the scaling factor of the y axis
    proj[1][1] *= -1;
    return proj;
}

glm::vec2 CameraNode::GetJitter() {
    return jitter;
}

void CameraNode::NextJitter() {
    if (useJitter) {
        jitterIndex = (jitterIndex + 1) % 16;
        jitter = glm::vec2(Halton(jitterIndex + 1, 2), Halton(jitterIndex + 1, 3));
        jitter = 2.0f * jitter - 1.0f;
        jitter /= extent;
    } else {
        jitter = glm::vec2(0, 0);
    }
}

void AssetManager::RequestLoadProject(const std::filesystem::path& path, const std::filesystem::path& binPath) {
    impl->requestedProjectPath = path;
    impl->requestedBinPath = binPath;
}

bool AssetManager::HasLoadRequest() const {
    return !impl->requestedProjectPath.empty() && !impl->requestedBinPath.empty();
}

void AssetManager::LoadRequestedProject() {
    LoadProject(impl->requestedProjectPath, impl->requestedBinPath);
    impl->requestedProjectPath = std::filesystem::path();
    impl->requestedBinPath = std::filesystem::path();
}

std::string AssetManager::GetProjectName() {
    return impl->currentProjectPath.stem().string();
}

std::filesystem::path AssetManager::GetCurrentProjectPath() {
    return impl->currentProjectPath;
}

std::filesystem::path AssetManager::GetCurrentBinPath() {
    return impl->currentBinPath;
}