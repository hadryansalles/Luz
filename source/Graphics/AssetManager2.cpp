#include "Luzpch.hpp"

#include "AssetManager2.hpp"

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <random>

#define SERIALIZE(name, var) if (dir == 0) { if (j.contains(name)) {from_json(j.at(name), var);}} else {to_json(j[name], var);}

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

MeshNode::MeshNode() {
    type = ObjectType::MeshNode;
}

void TextureAsset::Serialize(Serializer& s) {
    s("data", data);
    s("width", width);
    s("height", height);
    s("channels", channels);
}

void MeshAsset::Serialize(Serializer& s) {
    s("vertices", vertices);
    s("indices", indices);
}

void MaterialAsset::Serialize(Serializer& s) {
    //s("color", color);
    //s("emission", emission);
    s("metallic", metallic);
    s("roughness", roughness);
    //s("colorMap", colorMap);
    //s("aoMap", aoMap);
    //s("emissionMap", emissionMap);
    //s("normalMap", normalMap);
    //s("metallicRoughnessMap", metallicRoughnessMap);
}

void SceneAsset::Serialize(Serializer& s) {
    s("nodes", nodes);
}

void Node::Serialize(Serializer& s) {
    //s("children", children);
    //s("position", position);
    //s("rotation", rotation);
    //s("scale", scale);
}

void MeshNode::Serialize(Serializer& s) {
    //s("mesh", mesh);
    //s("material", material);
}

void AssetManager2::Serialize(Json& j, int dir) {
    // if (dir == 1) {
    //     for (auto& assetPair : assets) {
    //         Json& assetJson = j[std::to_string(assetPair.first)];
    //         assetPair.second->Serialize(assetJson, dir);
    //     }
    // }
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