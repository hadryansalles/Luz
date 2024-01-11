#include "Luzpch.hpp"

#include "AssetManager2.hpp"

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <random>

void to_json(Json& j, const glm::vec3& v) {
    j = Json{v.x, v.y, v.z};
}

void to_json(Json& j, const glm::vec4& v) {
    j = Json{v.x, v.y, v.z, v.w};
}

void from_json(const Json& j, glm::vec4& v) {
    if (j.is_array() && j.size() == 4) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
        v.w = j[3];
    }
}

void from_json(const Json& j, glm::vec3& v) {
    if (j.is_array() && j.size() == 3) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
    }
}

#define SERIALIZE(name, var) if (dir == 0) { if (j.contains(name)) {from_json(j.at(name), var);}} else {to_json(j[name], var);}

namespace {

std::string const base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline bool IsBase64(unsigned char c) {
    return (c == 43 || // +
        (c >= 47 && c <= 57) || // /-9
        (c >= 65 && c <= 90) || // A-Z
        (c >= 97 && c <= 122)); // a-z
}

std::string EncodeBase64(unsigned char const* input, size_t len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++) {
                ret += base64Chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
            ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
            ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) {
            ret += base64Chars[char_array_4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

inline std::vector<u8> DecodeBase64(std::string const& input) {
    size_t in_len = input.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<u8> ret;

    while (in_len-- && (input[in_] != '=') && IsBase64(input[in_])) {
        char_array_4[i++] = input[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = static_cast<unsigned char>(base64Chars.find(char_array_4[i]));
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64Chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret.push_back(char_array_3[j]);
        }
    }

    return ret;
}
}

Asset::~Asset()
{}

TextureAsset::TextureAsset() {
    type = AssetType::Texture;
}

MeshAsset::MeshAsset() {
    type = AssetType::Mesh;
}

MaterialAsset::MaterialAsset() {
    type = AssetType::Material;
}

SceneAsset::SceneAsset() {
    type = AssetType::Scene;
}

void Asset::Serialize(Json& j, int dir) {
    SERIALIZE("UUID", uuid);
    SERIALIZE("name", name);
    SERIALIZE("type", type);
}

void TextureAsset::Serialize(Json& j, int dir) {
    Asset::Serialize(j, dir);
    std::string dataBase64 = EncodeBase64(data.data(), data.size());
    SERIALIZE("data", dataBase64);
    SERIALIZE("width", width);
    SERIALIZE("height", height);
    SERIALIZE("channels", channels);
    if (dir == 0) {
        data = DecodeBase64(dataBase64);
    }
}

void MeshAsset::Serialize(Json& j, int dir) {
    Asset::Serialize(j, dir);
    std::string vertexBase64 = EncodeBase64((u8*)vertices.data(), vertices.size() * sizeof(MeshVertex));
    std::string indexBase64 = EncodeBase64((u8*)indices.data(), indices.size() * sizeof(u32));
    SERIALIZE("vertex", vertexBase64);
    SERIALIZE("index", indexBase64);
    int vertexCount = vertices.size();
    int indexCount = indices.size();
    SERIALIZE("vertexCount", vertexCount);
    SERIALIZE("indexCount", indexCount);
    if (dir == 0) {
        std::vector<u8> vertexData = DecodeBase64(vertexBase64);
        std::vector<u8> indexData = DecodeBase64(indexBase64);
        vertices.resize(vertexCount);
        indices.resize(vertexCount);
        memcpy(vertices.data(), vertexData.data(), vertexData.size());
        memcpy(indices.data(), indexData.data(), indexData.size());
    }
}

void MaterialAsset::Serialize(Json& j, int dir) {
    Asset::Serialize(j, dir);
    glm::vec3 color = glm::vec4(1.0f);
    glm::vec3 emission = glm::vec3(1.0f);
    f32 metallic = 1;
    f32 roughness = 1;
    AssetManager2::Instance().SerializeAsset(colorMap, j["colorMap"], dir);
    AssetManager2::Instance().SerializeAsset(aoMap, j["aoMap"], dir);
    AssetManager2::Instance().SerializeAsset(emissionMap, j["emissionMap"], dir);
    AssetManager2::Instance().SerializeAsset(normalMap, j["normalMap"], dir);
    AssetManager2::Instance().SerializeAsset(metallicRoughnessMap, j["metallicRoughnessMap"], dir);
}

void SceneAsset::Serialize(Json& j, int dir) {

}

void Node::Serialize(Json& j, int dir) {

}

void MeshNode::Serialize(Json& j, int dir) {

}

void AssetManager2::Serialize(Json& j, int dir) {
    if (dir == 1) {
        for (auto& assetPair : assets) {
            Json& assetJson = j[std::to_string(assetPair.first)];
            assetPair.second->Serialize(assetJson, dir);
        }
    }
}

void AssetManager2::Import(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (IsTexture(path)) {
        ImportTexture(path);
    }
}

void AssetManager2::ImportTexture(const std::filesystem::path& path) {
    auto t = CreateAsset<TextureAsset>(path.stem().string());
    u8* indata = stbi_load(path.string().c_str(), &t->width, &t->height, &t->channels, 4);
    t->data.resize(t->width * t->height * t->channels);
    memcpy(t->data.data(), indata, t->data.size());
}

void AssetManager2::ImportScene(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".gltf" || ext == ".glb") {
        ImportSceneGLTF(path);
    } else if (ext == ".obj") {
        ImportSceneOBJ(path);
    }
}

void AssetManager2::ImportSceneGLTF(const std::filesystem::path& path) {
    
}

void AssetManager2::ImportSceneOBJ(const std::filesystem::path& path) {
    std::string filename = path.stem().string().c_str();
    DEBUG_TRACE("Start loading mesh {}", path.string().c_str());
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::filesystem::path parentPath = path.parent_path();
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(), parentPath.string().c_str())) {
        LOG_ERROR("{} {}", warn, err);
        LOG_ERROR("Failed to load obj file {}", path.string().c_str());
    }
    // convert obj material to my material
    auto avg = [](const tinyobj::real_t value[3]) {return (value[0] + value[1] + value[2]) / 3.0f; };
    std::vector<Ref<MaterialAsset>> materialAssets;
    for (size_t i = 0; i < materials.size(); i++) {
        Ref<MaterialAsset> materialAsset = CreateAsset<MaterialAsset>(filename + ":" + materials[i].name);
        materialAsset->color = glm::vec4(glm::make_vec3(materials[i].diffuse), 1);
        materialAsset->emission  = glm::make_vec3(materials[i].emission);
        materialAsset->metallic  = materials[i].metallic;
        if (materials[i].specular != 0) {
            materialAsset->roughness = 1.0f - avg(materials[i].specular);
        }
        materialAsset->roughness = materials[i].roughness;
        materialAssets.push_back(materialAsset);
    }
    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    Ref<SceneAsset> scene = CreateAsset<SceneAsset>(filename);
    for (size_t i = 0; i < shapes.size(); i++) {
        // todo: remove unused vertices
        Ref<MeshAsset> mesh = CreateAsset<MeshAsset>(shapes[i].name);
        mesh->vertices.resize(attrib.vertices.size());
        for (int j = 0; j < attrib.vertices[j]; j+=3) {
            mesh->vertices[j / 3].position = {
                attrib.vertices[j + 0],
                attrib.vertices[j + 1],
                attrib.vertices[j + 2]
            };
        }
        mesh->indices.reserve(shapes[i].mesh.indices.size());
        for (int j = 0; j < shapes[i].mesh.indices.size(); j++) {
            mesh->indices[j] = shapes[i].mesh.indices[i].vertex_index;
        }
        Ref<MeshNode> node = scene->Add<MeshNode>();
        node->name = mesh->name;
        node->mesh = mesh;
        node->material = materialAssets.size() > 0 ? materialAssets[0] : CreateAsset<MaterialAsset>(mesh->name + "_material");
    }
}

bool AssetManager2::IsTexture(const std::filesystem::path& path) const {
    const std::string ext = path.extension().string();
    return ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}

bool AssetManager2::IsScene(const std::filesystem::path& path) const {
    const std::string ext = path.extension().string();
    return ext == ".obj" || ext == ".gltf" || ext == ".glb";
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