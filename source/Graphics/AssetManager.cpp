#include "Luzpch.hpp"

#include "AssetManager.hpp"

#include "SceneManager.hpp"
#include "MeshManager.hpp"
#include "Model.hpp"

#include <algorithm>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void AssetManager::Create() {

}

void AssetManager::Destroy() {

}

std::vector<ModelDesc> AssetManager::LoadMeshFile(std::filesystem::path path) {
    LUZ_PROFILE_FUNC();
    DEBUG_TRACE("Start loading mesh {}", path.string().c_str());
    std::string extension = path.extension().string();
    if (extension == ".obj" || extension == ".OBJ") {
        return AssetManager::LoadObjFile(path);
    }
    else {
        LOG_ERROR("Asset file format not supported!");
        return std::vector<ModelDesc>();
    }
}

bool AssetManager::IsMeshFile(std::filesystem::path path) {
    std::string extension = path.extension().string();
    return extension == ".obj" || extension == ".OBJ";
}

std::vector<ModelDesc> AssetManager::LoadObjFile(std::filesystem::path path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::filesystem::path parentPath = path.parent_path();
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(), parentPath.string().c_str())) {
        LOG_ERROR("{} {}", warn, err);
        LOG_ERROR("Failed to load obj file {}", path.string().c_str());
    }
    std::vector<TextureDesc> diffuseTextures(materials.size());
    for (size_t i = 0; i < materials.size(); i++) {
        if (materials[i].diffuse_texname != "") {
            std::filesystem::path copyPath = parentPath;
            diffuseTextures[i] = AssetManager::LoadImageFile(copyPath.append(materials[i].diffuse_texname));
        }
    }
    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    Collection* collection = SceneManager::CreateCollection();
    collection->name = path.filename().string();
    std::vector<ModelDesc> models;
    for (size_t i = 0; i < shapes.size(); i++) {
        MeshDesc* desc = new MeshDesc;
        std::unordered_map<MeshVertex, uint32_t> uniqueVertices{};
        int splittedShapeIndex = 0;
        size_t j = 0;
        size_t lastMaterialId = shapes[i].mesh.material_ids[0];
        for (const auto& index : shapes[i].mesh.indices) {
            MeshVertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = (uint32_t)(desc->vertices.size());
                desc->vertices.push_back(vertex);
            }
            
            desc->indices.push_back(uniqueVertices[vertex]);
            j += 1;

            if (j % 3 == 0) {
                size_t faceId = j / 3;
                if (faceId >= shapes[i].mesh.material_ids.size() || shapes[i].mesh.material_ids[faceId] != lastMaterialId) {
                    ModelDesc model;
                    desc->path = path;
                    desc->name = shapes[i].name + "_" + std::to_string(splittedShapeIndex);
                    model.mesh = desc;
                    model.collection = collection;
                    if (lastMaterialId != -1 && materials[lastMaterialId].diffuse_texname != "") {
                        model.texture = diffuseTextures[lastMaterialId];
                    }
                    models.push_back(model);
                    if (faceId < shapes[i].mesh.material_ids.size()) {
                        lastMaterialId = shapes[i].mesh.material_ids[faceId];
                    }
                    uniqueVertices.clear();
                    desc = new MeshDesc;
                }
            }
        }
        DEBUG_ASSERT(desc->vertices.size() == 0, "Reach shapes iteration ending without creating Model.");
        delete desc;
    }

    return models;
}

void AssetManager::AddObjFileToScene(std::filesystem::path path, Collection* parent) {
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
    std::vector<TextureDesc> diffuseTextures(materials.size());
    for (size_t i = 0; i < materials.size(); i++) {
        if (materials[i].diffuse_texname != "") {
            std::filesystem::path copyPath = parentPath;
            diffuseTextures[i] = AssetManager::LoadImageFile(copyPath.append(materials[i].diffuse_texname));
        }
    }
    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    Collection* collection = SceneManager::CreateCollection(parent);
    collection->name = path.filename().string();
    for (size_t i = 0; i < shapes.size(); i++) {
        MeshDesc* desc = new MeshDesc;
        std::unordered_map<MeshVertex, uint32_t> uniqueVertices{};
        int splittedShapeIndex = 0;
        size_t j = 0;
        size_t lastMaterialId = shapes[i].mesh.material_ids[0];
        for (const auto& index : shapes[i].mesh.indices) {
            MeshVertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

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
                uniqueVertices[vertex] = (uint32_t)(desc->vertices.size());
                desc->vertices.push_back(vertex);
            }
            
            desc->indices.push_back(uniqueVertices[vertex]);
            j += 1;

            if (j % 3 == 0) {
                size_t faceId = j / 3;
                if (faceId >= shapes[i].mesh.material_ids.size() || shapes[i].mesh.material_ids[faceId] != lastMaterialId) {
                    ModelDesc model;
                    desc->path = path;
                    desc->name = shapes[i].name + "_" + std::to_string(splittedShapeIndex);
                    model.mesh = desc;
                    model.collection = collection;
                    if (lastMaterialId != -1 && materials[lastMaterialId].diffuse_texname != "") {
                        model.texture = diffuseTextures[lastMaterialId];
                    }
                    SceneManager::AddPreloadedModel(model);
                    if (faceId < shapes[i].mesh.material_ids.size()) {
                        lastMaterialId = shapes[i].mesh.material_ids[faceId];
                    }
                    uniqueVertices.clear();
                    desc = new MeshDesc;
                }
            }
        }
        DEBUG_ASSERT(desc->vertices.size() == 0, "Reach shapes iteration ending without creating Model.");
        delete desc;
    }
}

TextureDesc AssetManager::LoadImageFile(std::filesystem::path path) {
    int texWidth, texHeight, texChannels;
    TextureDesc desc;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("Failed to load image file {}", path.string().c_str());
        return desc;
    }

    desc.data = pixels;
    desc.width = texWidth;
    desc.height = texHeight;
    desc.path = path;

    return desc;
}