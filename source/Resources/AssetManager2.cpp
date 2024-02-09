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

glm::mat4 Node::ComposeTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl, glm::mat4 parent) {
    glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rot)));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 scaleMat = glm::scale(scl);
    return parent * (translationMat * scaleMat * rotationMat);
}

glm::mat4 Node::GetLocalTransform() {
    return ComposeTransform(position, rotation, scale);
}

glm::mat4 Node::GetWorldTransform() {
    return parentTransform * GetLocalTransform();
}

glm::vec3 Node::GetWorldPosition() {
    return parentTransform * glm::vec4(position, 1);
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
    s("lightType", type);
    s("shadows", shadows);
}

Ref<SceneAsset> AssetManager2::GetInitialScene() {
    if (!initialScene) {
        CreateAsset<SceneAsset>("DefaultScene");
    }
    return Get<SceneAsset>(initialScene);
}

void AssetManager2::LoadProject(const std::filesystem::path& path) {
    Json j;
    int dir = Serializer::LOAD;
    j = Json::parse(AssetIO::ReadFile(path));
    for (auto& assetJson : j["assets"]) {
        Ref<Asset> asset;
        Serializer s = Serializer(assetJson, dir, *this);
        s.Serialize(asset);
    }
    for (auto& assetPair : assets) {
        LOG_INFO("uuid {} type {}", assetPair.second->uuid, assetPair.second->type);
    }
    initialScene = j["initialScene"];
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
        Serializer s = Serializer(assetJson, dir, *this);
        s.Serialize(asset);
        j["assets"].push_back(assetJson);
    }
    j["initialScene"] = initialScene;
    AssetIO::WriteFile(path, j.dump(2));
    // todo: use imgui flag to tell it's unsaved
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

void AssetManager2::AddAssetsToScene(Ref<SceneAsset>& scene, const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        UUID uuid = AssetIO::Import(path, *this);
        if (uuid != 0 && assets[uuid]->type == ObjectType::SceneAsset) {
            scene->Merge(*this, Get<SceneAsset>(uuid));
        }
    }
}

void SceneAsset::Merge(AssetManager2& manager, const Ref<SceneAsset>& rhs) {
    for (auto& node : rhs->nodes) {
        Ref<Object> newObject = manager.CloneObject(node->type, std::dynamic_pointer_cast<Object>(node));
        Ref<Node> newNode = std::dynamic_pointer_cast<Node>(newObject);
        newNode->children.clear();
        for (auto& child : node->children) {
            newNode->AddClone(manager, child);
        }
        nodes.push_back(newNode);
    }
}

Ref<Node> Node::Clone(Ref<Node>& node) {
    Ref<Object> cloneObject = AssetManager2::CloneObject(node->type, std::dynamic_pointer_cast<Object>(node));
    Ref<Node> clone = std::dynamic_pointer_cast<Node>(cloneObject);
    clone->children.clear();
    for (auto& child : node->children) {
        clone->Add(Node::Clone(child));
    }
    return clone;
}

void Node::AddClone(AssetManager2& manager, const Ref<Node>& node) {
    Ref<Object> newObject = manager.CloneObject(node->type, std::dynamic_pointer_cast<Object>(node));
    Ref<Node> newNode = std::dynamic_pointer_cast<Node>(newObject);
    newNode->children.clear();
    for (auto& child : node->children) {
        newNode->AddClone(manager, child);
    }
    children.push_back(newNode);
}