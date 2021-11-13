#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
#include "LogicalDevice.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <imgui/imgui_impl_vulkan.h>

void AssetManager::Setup() {
    LoadTexture("assets/default.png");
    LoadTexture("assets/white.png");
}

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
    texturesLock.lock();
    for (RID i = 0; i < nextTextureRID; i++) {
        DestroyTexture(textures[i]);
        unintializedTextures.push_back(i);
    }
    texturesLock.unlock();
}

void AssetManager::Finish() {
    meshesLock.lock();
    for (RID i = 0; i < nextMeshRID; i++) {
        meshDescs[i].indices.clear();
        meshDescs[i].vertices.clear();
        meshDescs[i] = MeshDesc();
    }
    nextMeshRID = 0;
    meshesLock.unlock();
    texturesLock.lock();
    for (RID i = 0; i < nextTextureRID; i++) {
        stbi_image_free(textureDescs[i].data);
        textureDescs[i] = TextureDesc();
    }
    nextTextureRID = 0;
    texturesLock.unlock();
}

RID AssetManager::NewMesh() {
    meshesLock.lock();
    RID rid = nextMeshRID++;
    unintializedMeshes.push_back(rid);
    meshesLock.unlock();
    return rid;
}

RID AssetManager::NewTexture() {
    texturesLock.lock();
    RID rid = nextTextureRID++;
    unintializedTextures.push_back(rid);
    texturesLock.unlock();
    return rid;
}

void AssetManager::UpdateResources() {
    LUZ_PROFILE_FUNC();

    // add new loaded models to scene
    GetLoadedModels();

    // initialize new meshes
    meshesLock.lock();
    std::vector<RID> toInitialize = std::move(unintializedMeshes);
    unintializedMeshes.clear();
    meshesLock.unlock();
    for (RID rid : toInitialize) {
        InitializeMesh(rid);
    }

    // initialize new textures
    texturesLock.lock();
    toInitialize = std::move(unintializedTextures);
    texturesLock.unlock();
    for (RID rid : toInitialize) {
        InitializeTexture(rid);
    }
    UpdateTexturesDescriptor(toInitialize);
}

void AssetManager::UpdateTexturesDescriptor(std::vector<RID>& rids) {
    LUZ_PROFILE_FUNC();
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const RID count = rids.size();
    std::vector<VkDescriptorImageInfo> imageInfos(count);
    std::vector<VkWriteDescriptorSet> writes(count);

    VkDescriptorSet bindlessDescriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    DEBUG_ASSERT(bindlessDescriptorSet != VK_NULL_HANDLE, "Null bindless descriptor set!");

    for (int i = 0; i < count; i++) {
        RID rid = rids[i];
        DEBUG_ASSERT(textures[rid].sampler != VK_NULL_HANDLE, "Texture not loaded!");
        textures[rid].imguiRID = ImGui_ImplVulkan_AddTexture(textures[rid].sampler, textures[rid].image.view, layout);

        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = textures[rid].image.view;
        imageInfos[i].sampler = textures[rid].sampler;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = 0;
        writes[i].dstArrayElement = rid;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }
    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), count, writes.data(), 0, nullptr);
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

void AssetManager::InitializeTexture(RID id) {
    TextureResource& res = textures[id];
    TextureDesc& desc = textureDescs[id];
    CreateTexture(desc, res);
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
    std::vector<RID> diffuseTextures(materials.size());
    std::vector<RID> specularTextures(materials.size());
    for (size_t i = 0; i < materials.size(); i++) {
        if (materials[i].diffuse_texname != "") {
            std::filesystem::path copyPath = parentPath;
            diffuseTextures[i] = AssetManager::LoadTexture(copyPath.append(materials[i].diffuse_texname));
        }
        else {
            diffuseTextures[i] = 1;
        }
        if (materials[i].specular_texname != "") {
            std::filesystem::path copyPath = parentPath;
            specularTextures[i] = AssetManager::LoadTexture(copyPath.append(materials[i].specular_texname));
        }
        else {
            specularTextures[i] = 1;
        }
    }
    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    Collection* collection = nullptr;
    if (shapes.size() > 1) {
        collection = Scene::CreateCollection();
        collection->name = path.stem().string();
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
                    Model model;
                    desc.path = std::filesystem::absolute(path);
                    desc.name = shapes[i].name + "_" + std::to_string(splittedShapeIndex);
                    model.name = desc.name;
                    model.parent = collection;
                    RID meshRID = NewMesh();
                    meshDescs[meshRID] = std::move(desc);
                    RecenterMesh(meshRID);
                    model.mesh = meshRID;
                    model.transform.SetPosition(meshDescs[meshRID].center);
                    if (lastMaterialId != -1) {
                        auto diffuse = materials[lastMaterialId].diffuse;
                        auto specular = materials[lastMaterialId].specular;
                        model.block.values[0] = materials[lastMaterialId].shininess;
                        model.block.colors[0] = glm::vec4(diffuse[0], diffuse[1], diffuse[2], 1.0);
                        model.block.colors[1] = glm::vec4(specular[0], specular[1], specular[2], 1.0);
                        model.block.textures[0] = diffuseTextures[lastMaterialId];
                        model.block.textures[1] = specularTextures[lastMaterialId];
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

RID AssetManager::LoadTexture(std::filesystem::path path) {
    // check if texture is already loaded
    RID rid = 0;
    path = std::filesystem::absolute(path);
    texturesLock.lock();
    for (RID i = 0; i < nextTextureRID; i++) {
        if (textureDescs[i].path == path) {
            rid = i;
            break;
        }
    }
    texturesLock.unlock();
    if (rid != 0) {
        return rid;
    }

    // read texture file
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("Failed to load image file {}", path.string().c_str());
        return 0;
    }

    // create texture desc
    rid = NewTexture();
    TextureDesc& desc = textureDescs[rid];
    desc.data = pixels;
    desc.width = texWidth;
    desc.height = texHeight;
    desc.path = path;

    return rid;
}

Model* AssetManager::LoadModel(std::filesystem::path path) {
    auto models = LoadModels(path);
    ASSERT(models.size() == 1, "LoadModel loaded more than one model.");
    return models[0];
}

std::vector<Model*> AssetManager::LoadModels(std::filesystem::path path) {
    if (loadedModels.size() != 0) {
        LOG_WARN("Sync load models with loaded models waiting to fetch...");
    }
    LoadOBJ(path);
    return GetLoadedModels();
}

std::vector<Model*> AssetManager::GetLoadedModels() {
    loadedModelsLock.lock();
    std::vector<Model> models = std::move(loadedModels);
    loadedModels.clear();
    loadedModelsLock.unlock();
    std::vector<Model*> newModels(models.size());
    for (int i = 0; i < models.size(); i++) {
        newModels[i] = Scene::CreateModel(models[i]);
    }
    return newModels;
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
    if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen)) { 
        DirOnImgui(std::filesystem::path("assets").parent_path());
    }
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
    if (ImGui::CollapsingHeader("Textures")) {
        for (RID rid = 0; rid < nextTextureRID; rid++) {
            ImGui::PushID(rid);
            if (ImGui::TreeNode(textureDescs[rid].path.stem().string().c_str())) {
                DrawTextureOnImgui(textures[rid]);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    std::string texturePath = textureDescs[rid].path.string();
                    ImGui::SetDragDropPayload("texture", texturePath.c_str(), texturePath.size());
                    DrawTextureOnImgui(textures[rid]);
                    ImGui::EndDragDropSource();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}