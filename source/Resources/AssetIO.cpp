#include "Luzpch.hpp"

#include "AssetIO.hpp"
#include "AssetManager2.hpp"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace std {
    template<> struct hash<MeshAsset::MeshVertex> {
        size_t operator()(MeshAsset::MeshVertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

namespace AssetIO {

void ImportSceneGLTF(const std::filesystem::path& path);
void ImportSceneOBJ(const std::filesystem::path& path);

bool IsTexture(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}

bool IsScene(const std::filesystem::path& path){
    const std::string ext = path.extension().string();
    return ext == ".obj" || ext == ".gltf" || ext == ".glb";
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

void WriteFile(const std::filesystem::path& path, const std::string& content) {
    FILE* f = fopen(path.string().c_str(), "w");
    fwrite(content.data(), sizeof(u8), content.size(), f);
    fclose(f);
}

void Import(const std::filesystem::path& path) {
    TimeScope t("AssetIO::Import(" + path.string() + ")");
    const std::string ext = path.extension().string();
    if (IsTexture(path)) {
        ImportTexture(path);
    } else if (IsScene(path)) {
        ImportScene(path);
    }
}

void ImportTexture(const std::filesystem::path& path, Ref<TextureAsset>& t) {
    u8* indata = stbi_load(path.string().c_str(), &t->width, &t->height, &t->channels, 4);
    t->data.resize(t->width * t->height * t->channels);
    memcpy(t->data.data(), indata, t->data.size());
}

void ImportTexture(const std::filesystem::path& path) {
    auto t = AssetManager2::Instance().CreateAsset<TextureAsset>(path.stem().string());
    ImportTexture(path, t);
}

void ImportScene(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".gltf" || ext == ".glb") {
        ImportSceneGLTF(path);
    } else if (ext == ".obj") {
        ImportSceneOBJ(path);
    }
}

void ImportSceneGLTF(const std::filesystem::path& path) {
    
}

void ImportSceneOBJ(const std::filesystem::path& path) {
    DEBUG_TRACE("Start loading mesh {}", path.string().c_str());
    auto& manager = AssetManager2::Instance();
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string warn;
    std::string filename = path.stem().string();
    std::string parentPath = path.parent_path().string() + "/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(),parentPath.c_str(), true)) {
        LOG_ERROR("{}", err);
        LOG_ERROR("Failed to load obj file {}", path.string().c_str());
    }

    if (err != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), err);
    }

    // convert obj material to my material
    auto avg = [](const tinyobj::real_t value[3]) {return (value[0] + value[1] + value[2]) / 3.0f; };
    std::vector<Ref<MaterialAsset>> materialAssets;
    for (size_t i = 0; i < materials.size(); i++) {
        Ref<MaterialAsset> asset = manager.CreateAsset<MaterialAsset>(filename + ":" + materials[i].name);
        materialAssets.push_back(asset);
        asset->color = glm::vec4(glm::make_vec3(materials[i].diffuse), 1);
        asset->emission = glm::make_vec3(materials[i].emission);
        asset->metallic = materials[i].metallic;
        if (materials[i].specular != 0) {
            asset->roughness = 1.0f - avg(materials[i].specular);
        } else {
            asset->roughness = materials[i].roughness;
        }
        if (materials[i].diffuse_texname != "") {
            asset->colorMap = manager.CreateAsset<TextureAsset>(materials[i].diffuse_texname);
            ImportTexture(parentPath + materials[i].diffuse_texname, asset->colorMap);
        }
    }

    Ref<SceneAsset> scene = manager.CreateAsset<SceneAsset>(filename);
    for (size_t i = 0; i < shapes.size(); i++) {
        std::unordered_map<MeshAsset::MeshVertex, uint32_t> uniqueVertices{};
        int splittedShapeIndex = 0;
        size_t j = 0;
        size_t lastMaterialId = shapes[i].mesh.material_ids[0];
        Ref<MeshAsset> asset = manager.CreateAsset<MeshAsset>(filename + ":" + shapes[i].name);
        for (const auto& index : shapes[i].mesh.indices) {
            MeshAsset::MeshVertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index != -1) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            }

            if (index.texcoord_index == -1) {
                vertex.texCoord = { 0, 0 };
            }
            else {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = (uint32_t)(asset->vertices.size());
                asset->vertices.push_back(vertex);
            }
            
            asset->indices.push_back(uniqueVertices[vertex]);
            j += 1;

            if (j % 3 == 0) {
                size_t faceId = j / 3;
                if (faceId >= shapes[i].mesh.material_ids.size() || shapes[i].mesh.material_ids[faceId] != lastMaterialId) {
                    asset->name += "_" + std::to_string(splittedShapeIndex);
                    Ref<MeshNode> model = manager.CreateObject<MeshNode>(asset->name);
                    scene->nodes.push_back(model);
                    model->mesh = asset;
                    if (lastMaterialId != -1) {
                        model->material = materialAssets[lastMaterialId];
                    }
                    if (faceId < shapes[i].mesh.material_ids.size()) {
                        lastMaterialId = shapes[i].mesh.material_ids[faceId];
                    }
                    uniqueVertices.clear();
                }
            }
        }
    }
}

}