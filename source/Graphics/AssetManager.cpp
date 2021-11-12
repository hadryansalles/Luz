#include "Luzpch.hpp"

#include "AssetManager.hpp"

#include <algorithm>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void AssetManager::Create() {
    LUZ_PROFILE_FUNC();
    UpdateResources();
}

void AssetManager::Destroy() {
    meshesLock.lock();
    for (RID i = 0; i < nextMeshRID; i++) {
        BufferManager::Destroy(meshes[i].vertexBuffer);
        BufferManager::Destroy(meshes[i].indexBuffer);
        unintializedMeshes.push_back(i);
    }
    meshesLock.unlock();
}

void AssetManager::Finish() {
    for (RID i = 0; i < nextMeshRID; i++) {
        meshDescs[i].indices.clear();
        meshDescs[i].vertices.clear();
        meshDescs[i] = MeshDesc();
    }
    nextMeshRID = 0;
}

RID AssetManager::NewMesh() {
    meshesLock.lock();
    RID rid = nextMeshRID++;
    unintializedMeshes.push_back(rid);
    meshesLock.unlock();
    return rid;
}

void AssetManager::UpdateResources() {
    meshesLock.lock();
    std::vector<RID> toInitialize = std::move(unintializedMeshes);
    unintializedMeshes.clear();
    meshesLock.unlock();
    for (RID rid : toInitialize) {
        InitializeMesh(rid);
    }
}

void AssetManager::InitializeMesh(RID rid) {
    MeshResource& res = meshes[rid];
    MeshDesc& desc = meshDescs[rid];
    res.indexCount = desc.indices.size();
    BufferManager::CreateVertexBuffer(res.vertexBuffer, desc.vertices.data(), sizeof(desc.vertices[0]) * desc.vertices.size());
    BufferManager::CreateIndexBuffer(res.indexBuffer, desc.indices.data(), sizeof(desc.indices[0]) * desc.indices.size());
}

void AssetManager::RecenterMesh(RID rid) {
    MeshResource& res = meshes[rid];
    MeshDesc& desc = meshDescs[rid];
    glm::vec3 p0 = glm::vec3(0);
    glm::vec3 p1 = glm::vec3(0);
    if (desc.vertices.size() < 1) {
        LOG_WARN("Setup mesh with 0 vertices...");
    }
    else {
        p0 = desc.vertices[0].pos;
        p1 = desc.vertices[0].pos;
    }
    for (int i = 0; i < desc.vertices.size(); i++) {
        if (desc.vertices[i].pos.x < p0.x) {
            p0.x = desc.vertices[i].pos.x;
        }
        if (desc.vertices[i].pos.y < p0.y) {
            p0.y = desc.vertices[i].pos.y;
        }
        if (desc.vertices[i].pos.z < p0.z) {
            p0.z = desc.vertices[i].pos.z;
        }
        if (desc.vertices[i].pos.x > p1.x) {
            p1.x = desc.vertices[i].pos.x;
        }
        if (desc.vertices[i].pos.y > p1.y) {
            p1.y = desc.vertices[i].pos.y;
        }
        if (desc.vertices[i].pos.z > p1.z) {
            p1.z = desc.vertices[i].pos.z;
        }
    }
    desc.center = (p0 + p1) / 2.0f;
    for (int i = 0; i < desc.vertices.size(); i++) {
        desc.vertices[i].pos -= desc.center;
    }
}

bool AssetManager::IsOBJ(std::filesystem::path path) {
    std::string extension = path.extension().string();
    return extension == ".obj" || extension == ".OBJ";
}

bool AssetManager::IsTexture(std::filesystem::path path) {
    std::string extension = path.extension().string();
    return extension == ".JPG" || extension == ".jpg" || extension == ".png" || extension == ".tga";
}

void AssetManager::LoadOBJ(std::filesystem::path path) {
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
            diffuseTextures[i] = AssetManager::LoadTexture(copyPath.append(materials[i].diffuse_texname));
        }
    }
    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    if (shapes.size() > 1) {
        // TODO: set collection
    }
    for (size_t i = 0; i < shapes.size(); i++) {
        MeshDesc desc;
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
                uniqueVertices[vertex] = (uint32_t)(desc.vertices.size());
                desc.vertices.push_back(vertex);
            }
            
            desc.indices.push_back(uniqueVertices[vertex]);
            j += 1;

            if (j % 3 == 0) {
                size_t faceId = j / 3;
                if (faceId >= shapes[i].mesh.material_ids.size() || shapes[i].mesh.material_ids[faceId] != lastMaterialId) {
                    ModelDesc model;
                    desc.path = std::filesystem::absolute(path);
                    desc.name = shapes[i].name + "_" + std::to_string(splittedShapeIndex);
                    model.name = desc.name;
                    RID meshRID = NewMesh();
                    meshDescs[meshRID] = std::move(desc);
                    RecenterMesh(meshRID);
                    model.mesh = meshRID;
                    if (lastMaterialId != -1 && materials[lastMaterialId].diffuse_texname != "") {
                        model.texture = diffuseTextures[lastMaterialId];
                    }
                    loadedModelsLock.lock();
                    loadedModels.push_back(model);
                    loadedModelsLock.unlock();
                    if (faceId < shapes[i].mesh.material_ids.size()) {
                        lastMaterialId = shapes[i].mesh.material_ids[faceId];
                    }
                    uniqueVertices.clear();
                }
            }
        }
    }
}

TextureDesc AssetManager::LoadTexture(std::filesystem::path path) {
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

std::vector<ModelDesc> AssetManager::LoadModels(std::filesystem::path path) {
    if (loadedModels.size() != 0) {
        LOG_WARN("Sync load models with loaded models waiting to fetch...");
    }
    LoadOBJ(path);
    return GetLoadedModels();
}

std::vector<ModelDesc> AssetManager::GetLoadedModels() {
    loadedModelsLock.lock();
    std::vector<ModelDesc> models = std::move(loadedModels);
    loadedModels.clear();
    loadedModelsLock.unlock();
    return models;
}

void AssetManager::AsyncLoadModels(std::filesystem::path path) {
    std::thread(LoadOBJ, path).detach();
}

void DirOnImgui(std::filesystem::path path) {
    if (ImGui::TreeNode(path.filename().string().c_str())) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                DirOnImgui(entry.path());
            }
            else {
                std::string filePath = entry.path().string();
                std::string fileName = entry.path().filename().string();
                ImGui::PushID(filePath.c_str());
                if (ImGui::Selectable(fileName.c_str())) {
                }
                if (AssetManager::IsOBJ(entry.path())) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("model", filePath.c_str(), filePath.size());
                        ImGui::Text(fileName.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                if (AssetManager::IsTexture(entry.path())) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("texture", filePath.c_str(), filePath.size());
                        ImGui::Text(fileName.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
}

void AssetManager::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    const float leftSpacing = ImGui::GetContentRegionAvailWidth()*1.0f/3.0f;
    // ImGui::Text("Path");
    // ImGui::SameLine(totalWidth*3.0f/5.0);
    // ImGui::Text("PATH TO SCENE");
    // ImGui::Text(SceneManager::GetPath().stem().string().c_str());
    // if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen)) {
    //     DirOnImgui(SceneManager::GetPath().parent_path());
    // }
    if (ImGui::CollapsingHeader("Meshes")) {
        for (int i = 0; i < nextMeshRID; i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(meshDescs[i].name.c_str())) {
                ImGui::Text("Path");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("path");
                ImGui::Text(meshDescs[i].path.string().c_str());
                ImGui::PopID();
                ImGui::Text("Vertices");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("verticesCount");
                ImGui::Text("%u", meshDescs[i].vertices.size());
                ImGui::PopID();
                ImGui::Text("Indices");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("indicesCount");
                ImGui::Text("%u", meshDescs[i].indices.size());
                ImGui::PopID();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}