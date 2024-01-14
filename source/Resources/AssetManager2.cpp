#include "Luzpch.hpp"

#include "AssetManager2.hpp"
#include "Serializer.hpp"
#include "AssetIO.hpp"

#include <random>

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

glm::mat4 Node::GetLocalTransform() {
    glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rotation)));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 scaleMat = glm::scale(scale);
    return translationMat * scaleMat * rotationMat;
}

glm::mat4 Node::GetWorldTransform() {
    return parentTransform * GetLocalTransform();
}

void Node::UpdateTransforms() {
    worldTransform = GetWorldTransform();
    for (auto& child : children) {
        child->parentTransform = worldTransform;
        child->UpdateTransforms();
    }
}

void SceneAsset::UpdateTransforms() {
    for (auto& node : nodes) {
        node->parentTransform = glm::mat4(1);
        node->UpdateTransforms();
    }
}

MeshNode::MeshNode() {
    type = ObjectType::MeshNode;
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
    s.Vector("nodes", nodes);
}

void Node::Serialize(Serializer& s) {
    s.Vector("children", children);
    s("position", position);
    s("rotation", rotation);
    s("scale", scale);
}

void MeshNode::Serialize(Serializer& s) {
    s.Asset("mesh", mesh);
    s.Asset("material", material);
}

Ref<SceneAsset> AssetManager2::GetInitialScene() {
    if (initialScene) {
        return Get<SceneAsset>(initialScene);
    }
}

void AssetManager2::LoadProject(const std::filesystem::path& path) {
    Json j;
    int dir = Serializer::LOAD;
    j = Json::parse(AssetIO::ReadFile(path));
    for (auto& assetJson : j) {
        Ref<Asset> asset;
        Serializer s = Serializer(assetJson, dir);
        s.Serialize(asset);
    }
    if (j.contains("initialScene")) {
        initialScene = j["initialScene"];
    }
}

void AssetManager2::SaveProject(const std::filesystem::path& path) {
    TimeScope t("AssetManager::SaveProject");
    Json j;
    int dir = Serializer::SAVE;
    std::vector<Ref<Asset>> assetsOrdered;
    assetsOrdered.reserve(assets.size());
    for (auto& assetPair : assets) {
        assetsOrdered.push_back(assetPair.second);
    }
    std::sort(assetsOrdered.begin(), assetsOrdered.end(), [&](const Ref<Asset>& a, const Ref<Asset>& b) {
        return a->type < b->type;
    });
    for (Ref<Asset> asset : assetsOrdered) {
        Json assetJson;
        Serializer s = Serializer(assetJson, dir);
        s.Serialize(asset);
        j.push_back(assetJson);
    }
    j["initialScene"] = initialScene;
}

void AssetManager2::OnImgui() {
     for (auto& assetPair : assets) {
         auto& asset = assetPair.second;
         if (ImGui::TreeNode(&asset->uuid, "%s", asset->name.c_str())) {
             ImGui::TreePop();
         }
     }
}

UUID AssetManager2::NewUUID() {
    // todo: replace with something actually UUID
    static std::random_device rd;
    static std::mt19937_64 eng(rd());
    static std::uniform_int_distribution<u64> dist(std::llround(std::pow(2,61)), std::llround(std::pow(2,62)));
    return dist(eng);
}

AssetManager2& AssetManager2::Instance() {
    static AssetManager2 instance;
    return instance;
}