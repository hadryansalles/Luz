#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "GraphicsPipelineManager.hpp"
//#include "LogicalDevice.hpp"
#include "VulkanLayer.h"
#include "RayTracing.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#// include <stb_image.h>
#include <tiny_gltf.h>

#include <imgui/imgui_impl_vulkan.h>

void AssetManager::Setup() {
}

void AssetManager::Create() {
    LUZ_PROFILE_FUNC();
    UpdateResources();
}

void AssetManager::Destroy() {
    meshesLock.lock();
    for (RID i = 0; i < nextMeshRID; i++) {
        meshes[i].vertexBuffer = {};
        meshes[i].indexBuffer = {};
        unintializedMeshes.push_back(i);
    }
    meshesLock.unlock();
    texturesLock.lock();
    for (RID i = 0; i < nextTextureRID; i++) {
        images[i] = {};
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
        delete textureDescs[i].data;
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
    if (toInitialize.size()) {
        RayTracing::CreateBLAS(toInitialize);
    }

    // initialize new textures
    texturesLock.lock();
    toInitialize = std::move(unintializedTextures);
    texturesLock.unlock();
    for (RID rid : toInitialize) {
        InitializeTexture(rid);
    }
}

void AssetManager::InitializeMesh(RID rid) {
    MeshResource& res = meshes[rid];
    MeshDesc& desc = meshDescs[rid];
    res.vertexCount = desc.vertices.size();
    res.indexCount = desc.indices.size();
    res.vertexBuffer = vkw::CreateBuffer(
        sizeof(MeshVertex) * desc.vertices.size(),
        vkw::BufferUsage::Vertex | vkw::BufferUsage::AccelerationStructureInput,
        vkw::Memory::GPU,
        ("VertexBuffer" + std::to_string(rid))
    );
    res.indexBuffer = vkw::CreateBuffer(
        sizeof(uint32_t) * desc.indices.size(),
        vkw::BufferUsage::Index | vkw::BufferUsage::AccelerationStructureInput,
        vkw::Memory::GPU,
        ("IndexBuffer" + std::to_string(rid))
    );
    vkw::BeginCommandBuffer(vkw::Queue::Transfer);
    vkw::CmdCopy(res.vertexBuffer, desc.vertices.data(), res.vertexBuffer.size);
    vkw::CmdCopy(res.indexBuffer, desc.indices.data(), res.indexBuffer.size);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Transfer);
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

    images[id] = vkw::CreateImage({
        .width = desc.width,
        .height = desc.height,
        .format = vkw::Format::RGBA8_unorm,
        .usage = vkw::ImageUsage::Sampled | vkw::ImageUsage::TransferDst,
        .name = "Texture " + std::to_string(id),
    });
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdBarrier(images[id], vkw::Layout::TransferDst);
    vkw::CmdCopy(images[id], desc.data, desc.width * desc.height * 4);
    vkw::CmdBarrier(images[id], vkw::Layout::ShaderRead);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);
}

bool AssetManager::IsModel(std::filesystem::path path) {
    return IsOBJ(path) || IsGLTF(path);
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
    // convert obj material to my material
    auto avg = [](const tinyobj::real_t value[3]) {return (value[0] + value[1] + value[2]) / 3.0f; };
    std::vector<MaterialBlock> materialBlocks(materials.size());
    for (size_t i = 0; i < materials.size(); i++) {
        materialBlocks[i].color = glm::vec4(glm::make_vec3(materials[i].diffuse), 1);
        materialBlocks[i].emission  = glm::make_vec3(materials[i].emission);
        materialBlocks[i].metallic  = materials[i].metallic;
        if (materials[i].specular != 0) {
            materialBlocks[i].roughness = 1.0f - avg(materials[i].specular);
        }
        materialBlocks[i].roughness = materials[i].roughness;
        // if (materials[i].diffuse_texname != "") {
        //     std::filesystem::path copyPath = parentPath;
        //     materialBlocks[i].colorMap = AssetManager::LoadTexture(copyPath.append(materials[i].diffuse_texname));
        //     materialBlocks[i].color = glm::vec3(1.0);
        // }
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
                        model.block.material = materialBlocks[lastMaterialId];
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

void AssetManager::LoadGLTF(std::filesystem::path path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    if (path.extension() == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    }
    else {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
    }

    if (!warn.empty()) {
      LOG_WARN("Warn: {}", warn.c_str());
    }

    if (!err.empty()) {
      LOG_ERROR("Err: {}", err.c_str());
    }

    if (!ret) {
      LOG_ERROR("Failed to parse glTF");
      return;
    }

    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    const auto getBuffer = [&](auto& accessor, auto& view) { return &model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset]; };

    std::vector<RID> loadedTextures(model.textures.size());
    for (int i = 0; i < model.textures.size(); i++) {

        tinygltf::Texture texture = model.textures[i];
        tinygltf::Image image = model.images[texture.source];
        unsigned char* buffer = new unsigned char[image.width * image.height * 4];
        if (image.component == 3) {
            unsigned char* rgba = buffer; 
            unsigned char* rgb = image.image.data();
            for (u32 j = 0; j < image.width * image.height; j++) {
                rgba[0] = rgb[0];
                rgba[1] = rgb[1];
                rgba[2] = rgb[2];
                rgba += 4;
                rgb += 3;
            }
        } else {
            memcpy(buffer, image.image.data(), image.width * image.height * 4);
        }

        loadedTextures[i] = NewTexture();
        TextureDesc& desc = textureDescs[loadedTextures[i]];
        desc.data = buffer;
        desc.width = image.width;
        desc.height = image.height;
        desc.path = texture.name;
    }

    std::vector<MaterialBlock> materials(model.materials.size());

    for (int i = 0; i < materials.size(); i++) {
        auto& mat = model.materials[i];
        // pbr
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            materials[i].colorMap = loadedTextures[mat.values["baseColorTexture"].TextureIndex()];
        }
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
            materials[i].metallicRoughnessMap = loadedTextures[mat.values["metallicRoughnessTexture"].TextureIndex()];
        }
        if (mat.values.find("baseColorFactor") != mat.values.end()) {
            materials[i].color = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.values.find("roughnessFactor") != mat.values.end()) {
            materials[i].roughness = mat.values["roughnessFactor"].Factor();
        }
        if (mat.values.find("metallicFactor") != mat.values.end()) {
            materials[i].metallic = mat.values["metallicFactor"].Factor();
        }
        // additional
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
            materials[i].normalMap = loadedTextures[mat.additionalValues["normalTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
            materials[i].emissionMap = loadedTextures[mat.additionalValues["emissiveTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
            materials[i].aoMap = loadedTextures[mat.additionalValues["occlusionTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
            materials[i].emission = glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data());
        }
    }

    Collection* sceneCollection = nullptr;
    if (model.meshes.size() > 1) {
        sceneCollection = Scene::CreateCollection();
        sceneCollection->name = path.stem().string();
    }
    for (const tinygltf::Mesh & mesh : model.meshes) {
        Collection* collection = nullptr;
        if (mesh.primitives.size() > 1) {
            collection = Scene::CreateCollection();
            if (model.nodes.size() > 0) {
                if (model.nodes[0].scale.size() == 3) {
                    glm::vec3 scale(model.nodes[0].scale[0], model.nodes[0].scale[1], model.nodes[0].scale[2]);
                    collection->transform.SetScale(scale);
                }
            }
            if (sceneCollection) {
                Scene::SetCollection(collection, sceneCollection);
            }
            collection->name = mesh.name != "" ? mesh.name : path.stem().string();
        }
        for (int i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& primitive = mesh.primitives[i];
            MeshDesc desc{};
            desc.name = mesh.name != "" ? mesh.name : path.stem().string();
            desc.name += "_" + std::to_string(i);
            desc.path = path;

            float* bufferPos = nullptr;
            float* bufferNormals = nullptr;
            float* bufferTangents = nullptr;
            float* bufferUV = nullptr;

            int stridePos = 0;
            int strideNormals = 0;
            int strideTangents = 0;
            int strideUV = 0;

            u32 vertexCount = 0;
            u32 indexCount = 0;

            // position
            {
                auto it = primitive.attributes.find("POSITION");
                DEBUG_ASSERT(it != primitive.attributes.end(), "Primitive don't have position attribute");
                const tinygltf::Accessor accessor = model.accessors[it->second];
                const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                bufferPos = (float*)getBuffer(accessor, bufferView);
                stridePos = accessor.ByteStride(bufferView) / sizeof(float);
                vertexCount = accessor.count;
            }

            // normal
            {
                auto it = primitive.attributes.find("NORMAL");
                if (it != primitive.attributes.end()) {
                    const tinygltf::Accessor accessor = model.accessors[it->second];
                    const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    bufferNormals = (float*)getBuffer(accessor, bufferView);
                    strideNormals = accessor.ByteStride(bufferView) / sizeof(float);
                }
            }

            // tangent
            {
                auto it = primitive.attributes.find("TANGENT");
                if (it != primitive.attributes.end()) {
                    const tinygltf::Accessor accessor = model.accessors[it->second];
                    const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    bufferTangents = (float*)getBuffer(accessor, bufferView);
                    strideTangents = accessor.ByteStride(bufferView) / sizeof(float);
                }
            }

            // uvs
            {
                auto it = primitive.attributes.find("TEXCOORD_0");
                if (it != primitive.attributes.end()) {
                    const tinygltf::Accessor accessor = model.accessors[it->second];
                    const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    bufferUV = (float*)getBuffer(accessor, bufferView);
                    strideUV = accessor.ByteStride(bufferView) / sizeof(float);
                }
            }

            // vertices
            for (u32 v = 0; v < vertexCount; v++) {
                MeshVertex vertex{};
                vertex.pos = glm::make_vec3(&bufferPos[v * stridePos]);
                vertex.normal = bufferNormals ? glm::make_vec3(&bufferNormals[v * strideNormals]) : glm::vec3(0);
                vertex.texCoord = bufferUV ? glm::make_vec3(&bufferUV[v * strideUV]) : glm::vec3(0);
                vertex.tangent = bufferTangents ? glm::make_vec4(&bufferTangents[v * strideTangents]) : glm::vec4(0);
                desc.vertices.push_back(vertex);
            }

            // indices
            DEBUG_ASSERT(primitive.indices > -1, "Non indexed primitive not supported!");
            {
                const tinygltf::Accessor accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                indexCount = accessor.count;
                auto pushIndices = [&](auto* bufferIndex) {
                    for (u32 i = 0; i < indexCount; i++) {
                        desc.indices.push_back(bufferIndex[i]);
                    }
                };
                switch (accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    pushIndices((u32*)getBuffer(accessor, bufferView));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    pushIndices((u16*)getBuffer(accessor, bufferView));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    pushIndices((u8*)getBuffer(accessor, bufferView));
                    break;
                default:
                    DEBUG_ASSERT(false, "Index type not supported!");
                }
            }

            // calculate tangents
            if (!bufferTangents) {
                glm::vec3* tan1 = new glm::vec3[desc.vertices.size()*2];
                glm::vec3* tan2 = tan1 + vertexCount;
                for (int indexID = 0; indexID < desc.indices.size(); indexID += 3) {
                    int index1 = desc.indices[indexID + 0];
                    int index2 = desc.indices[indexID + 2];
                    int index3 = desc.indices[indexID + 1];
                    MeshVertex& v1 = desc.vertices[index1];
                    MeshVertex& v2 = desc.vertices[index2];
                    MeshVertex& v3 = desc.vertices[index3];
                    glm::vec3 e1 = v2.pos - v1.pos;
                    glm::vec3 e2 = v3.pos - v1.pos;
                    glm::vec2 duv1 = v2.texCoord - v1.texCoord;
                    glm::vec2 duv2 = v3.texCoord - v1.texCoord;
                    float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);
                    glm::vec3 sdir = f * (duv2.y * e1 - duv1.y * e2);
                    glm::vec3 tdir = f * (duv1.x * e2 - duv2.x * e1);
                    tan1[index1] += sdir;
                    tan1[index2] += sdir;
                    tan1[index3] += sdir;
                    tan2[index1] += tdir;
                    tan2[index2] += tdir;
                    tan2[index3] += tdir;
                }
                for (int a = 0; a < desc.vertices.size(); a++) {
                    glm::vec3 t = tan1[a];
                    MeshVertex& v = desc.vertices[a];
                    glm::vec3 n = v.normal;
                    v.tangent = glm::vec4(glm::normalize(t - n * glm::dot(t, n)), 1.0);
                    v.tangent.w = (glm::dot(glm::cross(n, t), tan2[a]) < 0.0f) ? -1.0f : 1.0f;
                }
                delete[] tan1;
            }

            RID meshID = NewMesh();
            Model model;
            model.name = desc.name;
            model.mesh = meshID;
            model.parent = collection;
            if (!collection) {
                model.parent = sceneCollection;
            }
            if (primitive.material > -1) {
                model.block.material = materials[primitive.material];
            }
            loadedModelsLock.lock();
            loadedModels.push_back(model);
            loadedModelsLock.unlock();
            meshDescs[meshID] = std::move(desc);
        }
    }
}

RID AssetManager::CreateTexture(std::string name, u8* data, u32 width, u32 height) {
    RID rid = NewTexture();
    TextureDesc& desc = textureDescs[rid];
    desc.data = data;
    desc.width = width;
    desc.height = height;
    desc.path = name;
    return rid;
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

    if (texChannels == 3) {
        LOG_ERROR("RGB textures may not be supported!");
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
    if (IsOBJ(path)) { LoadOBJ(path); }
    else if (IsGLTF(path)) { LoadGLTF(path); }
    return GetLoadedModels();
}

std::vector<Model*> AssetManager::GetLoadedModels() {
    loadedModelsLock.lock();
    std::vector<Model> models = std::move(loadedModels);
    loadedModels.clear();
    loadedModelsLock.unlock();
    std::vector<Model*> newModels(models.size());
    for (int i = 0; i < models.size(); i++) {
        newModels[i] = Scene::CreateModel(&models[i]);
    }
    return newModels;
}

bool AssetManager::IsOBJ(std::filesystem::path path) {
    return path.extension() == ".obj";
}

bool AssetManager::IsGLTF(std::filesystem::path path) {
    return path.extension() == ".gltf" || path.extension() == ".glb";
}

void AssetManager::AsyncLoadModels(std::filesystem::path path) {
    if (IsOBJ(path)) { std::thread(LoadOBJ, path).detach(); }
    else if (IsGLTF(path)) { std::thread(LoadGLTF, path).detach(); }
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
                if (AssetManager::IsModel(entry.path())) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("mesh", filePath.c_str(), filePath.size());
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
    const float totalWidth = ImGui::GetContentRegionAvail().x;
    const float leftSpacing = ImGui::GetContentRegionAvail().x*1.0f/3.0f;
    if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen)) { 
        DirOnImgui(std::filesystem::path("assets"));
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
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("textureID", &rid, sizeof(RID*));
                    ImGui::Image(images[rid].imguiRID, ImVec2(256, 256));
                    ImGui::EndDragDropSource();
                }
                DrawTextureOnImgui(images[rid]);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("textureID", &rid, sizeof(RID*));
                    ImGui::Image(images[rid].imguiRID, ImVec2(256, 256));
                    ImGui::EndDragDropSource();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}