#include "Util.hpp"
#include "AssetIO.hpp"
#include "AssetManager.hpp"
#include "Log.hpp"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ostream>

namespace std {
    template<> struct hash<MeshAsset::MeshVertex> {
        size_t operator()(MeshAsset::MeshVertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

namespace AssetIO {

UUID ImportSceneGLTF(const std::filesystem::path& path, AssetManager& manager);
UUID ImportSceneOBJ(const std::filesystem::path& path, AssetManager& manager);

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

std::vector<u8> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<u8> bytes((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));
    input.close();
    return bytes;
}

void WriteFileBytes(const std::filesystem::path& path, const std::vector<u8>& content) {
    std::ofstream file(path, std::ofstream::binary);
    if (file.is_open()) {
        file.write((char*)content.data(), content.size());
        file.close();
    }
}

void WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ofstream::out);
    if (file.is_open()) {
        file.write(content.data(), content.size());
        file.close();
    }
}

UUID Import(const std::filesystem::path& path, AssetManager& assets) {
    TimeScope t("AssetIO::Import(" + path.string() + ")", true);
    const std::string ext = path.extension().string();
    if (IsTexture(path)) {
        return ImportTexture(path, assets);
    } else if (IsScene(path)) {
        return ImportScene(path, assets);
    }
    return 0;
}

void ReadTexture(const std::filesystem::path& path, std::vector<u8>& data, i32& w, i32& h) {
    i32 channels = 4;
    u8* indata = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    data.resize(w * h * 4);
    memcpy(data.data(), indata, data.size());
    stbi_image_free(indata);
}

void ImportTexture(const std::filesystem::path& path, Ref<TextureAsset>& t) {
    u8* indata = stbi_load(path.string().c_str(), &t->width, &t->height, &t->channels, 4);
    t->data.resize(t->width * t->height * 4);
    memcpy(t->data.data(), indata, t->data.size());
    t->channels = 4;
    stbi_image_free(indata);
}

UUID ImportTexture(const std::filesystem::path& path, AssetManager& assets) {
    auto t = assets.CreateAsset<TextureAsset>(path.stem().string());
    ImportTexture(path, t);
    return t->uuid;
}

UUID ImportScene(const std::filesystem::path& path, AssetManager& assets) {
    const std::string ext = path.extension().string();
    if (ext == ".gltf" || ext == ".glb") {
        return ImportSceneGLTF(path, assets);
    } else if (ext == ".obj") {
        return ImportSceneOBJ(path, assets);
    }
    return 0;
}

UUID ImportSceneGLTF(const std::filesystem::path& path, AssetManager& manager) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    if (path.extension() == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    } else {
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
      return 0;
    }

    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    const auto getBuffer = [&](auto& accessor, auto& view) { return &model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset]; };

    std::vector<Ref<TextureAsset>> loadedTextures(model.textures.size());
    for (int i = 0; i < model.textures.size(); i++) {
        tinygltf::Texture texture = model.textures[i];
        tinygltf::Image image = model.images[texture.source];
        std::vector<u8> buffer(image.width * image.height * 4);
        if (image.component == 3) {
            unsigned char* rgba = buffer.data(); 
            unsigned char* rgb = image.image.data();
            for (u32 j = 0; j < image.width * image.height; j++) {
                rgba[0] = rgb[0];
                rgba[1] = rgb[1];
                rgba[2] = rgb[2];
                rgba += 4;
                rgb += 3;
            }
        } else {
            memcpy(buffer.data(), image.image.data(), image.width * image.height * 4);
        }
        loadedTextures[i] = manager.CreateAsset<TextureAsset>(texture.name);
        loadedTextures[i]->data = buffer;
        loadedTextures[i]->width = image.width;
        loadedTextures[i]->height = image.height;
        loadedTextures[i]->channels = 4;
    }

    std::vector<Ref<MaterialAsset>> materials(model.materials.size());

    for (int i = 0; i < materials.size(); i++) {
        auto& mat = model.materials[i];
        materials[i] = manager.CreateAsset<MaterialAsset>(mat.name);
        // pbr
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            materials[i]->colorMap = loadedTextures[mat.values["baseColorTexture"].TextureIndex()];
        }
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
            materials[i]->metallicRoughnessMap = loadedTextures[mat.values["metallicRoughnessTexture"].TextureIndex()];
        }
        if (mat.values.find("baseColorFactor") != mat.values.end()) {
            materials[i]->color = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.values.find("roughnessFactor") != mat.values.end()) {
            materials[i]->roughness = (float)mat.values["roughnessFactor"].Factor();
        }
        if (mat.values.find("metallicFactor") != mat.values.end()) {
            materials[i]->metallic = (float)mat.values["metallicFactor"].Factor();
        }
        // additional
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
            materials[i]->normalMap = loadedTextures[mat.additionalValues["normalTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
            materials[i]->emissionMap = loadedTextures[mat.additionalValues["emissiveTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
            materials[i]->aoMap = loadedTextures[mat.additionalValues["occlusionTexture"].TextureIndex()];
        }
        if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
            materials[i]->emission = glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data());
        }
    }

    std::vector<Ref<MeshAsset>> loadedMeshes;
    std::vector<int> loadedMeshMaterials;
    for (const tinygltf::Mesh& mesh : model.meshes) {
        for (int i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& primitive = mesh.primitives[i];
            std::string name = (mesh.name != "" ? mesh.name : path.stem().string()) + "_" + std::to_string(i);
            Ref<MeshAsset>& desc = loadedMeshes.emplace_back(manager.CreateAsset<MeshAsset>(name));
            loadedMeshMaterials.emplace_back(primitive.material);

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
                MeshAsset::MeshVertex vertex{};
                vertex.position = glm::make_vec3(&bufferPos[v * stridePos]);
                vertex.normal = bufferNormals ? glm::make_vec3(&bufferNormals[v * strideNormals]) : glm::vec3(0);
                vertex.texCoord = bufferUV ? glm::make_vec3(&bufferUV[v * strideUV]) : glm::vec3(0);
                vertex.tangent = bufferTangents ? glm::make_vec4(&bufferTangents[v * strideTangents]) : glm::vec4(0);
                desc->vertices.push_back(vertex);
            }

            // indices
            DEBUG_ASSERT(primitive.indices > -1, "Non indexed primitive not supported!");
            {
                const tinygltf::Accessor accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                indexCount = accessor.count;
                auto pushIndices = [&](auto* bufferIndex) {
                    for (u32 i = 0; i < indexCount; i++) {
                        desc->indices.push_back(bufferIndex[i]);
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
                glm::vec3* tan1 = new glm::vec3[desc->vertices.size()*2];
                glm::vec3* tan2 = tan1 + vertexCount;
                for (int indexID = 0; indexID < desc->indices.size(); indexID += 3) {
                    int index1 = desc->indices[indexID + 0];
                    int index2 = desc->indices[indexID + 2];
                    int index3 = desc->indices[indexID + 1];
                    const auto& v1 = desc->vertices[index1];
                    const auto& v2 = desc->vertices[index2];
                    const auto& v3 = desc->vertices[index3];
                    glm::vec3 e1 = v2.position - v1.position;
                    glm::vec3 e2 = v3.position - v1.position;
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
                for (int a = 0; a < desc->vertices.size(); a++) {
                    glm::vec3 t = tan1[a];
                    auto& v = desc->vertices[a];
                    glm::vec3 n = v.normal;
                    v.tangent = glm::vec4(glm::normalize(t - n * glm::dot(t, n)), 1.0);
                    v.tangent.w = (glm::dot(glm::cross(n, t), tan2[a]) < 0.0f) ? -1.0f : 1.0f;
                }
                delete[] tan1;
            }
        }
    }

    std::vector<Ref<Node>> loadedNodes;
    for (const tinygltf::Node& node : model.nodes) {
        Ref<Node> groupNode = manager.CreateObject<Node>(node.name);
        if (node.mesh >= 0) {
            Ref<MeshNode> meshNode = manager.CreateObject<MeshNode>(node.name);
            meshNode->mesh = loadedMeshes[node.mesh];
            int matId = loadedMeshMaterials[node.mesh];
            if (matId >= 0) {
                meshNode->material = materials[matId];
            }
            Node::SetParent(meshNode, groupNode);
        }
        if (node.camera >= 0) {
            // todo: load camera
        }
        if (node.light >= 0) {
            Ref<LightNode> lightNode = manager.CreateObject<LightNode>(node.name);
            Node::SetParent(lightNode, groupNode);
        }
        if (node.translation.size() == 3) {
            groupNode->position = { float(node.translation[0]), float(node.translation[1]), float(node.translation[2]) };
        }
        if (node.rotation.size() == 4) {
            glm::quat quat = { float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2]) };
            groupNode->rotation = glm::degrees(glm::eulerAngles(quat));
        }
        if (node.scale.size() == 3) {
            groupNode->scale = { float(node.scale[0]), float(node.scale[1]), float(node.scale[2]) };
        }
        if (node.matrix.size() == 16) {
            glm::mat4 mat;
            for (int i = 0; i < 16; i++) {
                glm::value_ptr(mat)[i] = node.matrix[i];
            }
            glm::quat quat = {};
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(mat, groupNode->scale, quat, groupNode->position, skew, perspective);
            groupNode->rotation = glm::degrees(glm::eulerAngles(quat));
        }
        loadedNodes.emplace_back(groupNode);
    }
    for (int i = 0; i < model.nodes.size(); i++) {
        for (auto child : model.nodes[i].children) {
            Node::SetParent(loadedNodes[child], loadedNodes[i]);
        }
    }

    std::vector<Ref<SceneAsset>> loadedScenes;
    for (const tinygltf::Scene& scene : model.scenes) {
        Ref<SceneAsset> s = loadedScenes.emplace_back(manager.CreateAsset<SceneAsset>(scene.name));
        for (auto node : scene.nodes) {
            s->Add(loadedNodes[node]);
        }
    }

    return loadedScenes.size() ? loadedScenes[0]->uuid : 0;
}

UUID ImportSceneOBJ(const std::filesystem::path& path, AssetManager& manager) {
    DEBUG_TRACE("Start loading mesh {}", path.string().c_str());
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string warn;
    std::string filename = path.stem().string();
    std::string parentPath = path.parent_path().string() + "/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str(), parentPath.c_str(), true)) {
        LOG_ERROR("{}", err);
        LOG_WARN("{}", warn);
        LOG_ERROR("Failed to load obj file {}", path.string().c_str());
    }

    if (warn != "") {
        LOG_WARN("Warning during load obj file {}: {}", path.string().c_str(), warn);
    }

    // convert obj material to my material
    auto avg = [](const tinyobj::real_t value[3]) {return (value[0] + value[1] + value[2]) / 3.0f; };
    std::vector<Ref<MaterialAsset>> materialAssets;
    std::unordered_map<std::string, Ref<TextureAsset>> textureAssets;
    for (size_t i = 0; i < materials.size(); i++) {
        Ref<MaterialAsset> asset = manager.CreateAsset<MaterialAsset>(filename + ":" + materials[i].name);
        //asset->color = glm::vec4(1, 0, 0, 1);
        asset->color = glm::vec4(glm::make_vec3(materials[i].diffuse), 1);
        asset->emission = glm::make_vec3(materials[i].emission);
        asset->metallic = materials[i].metallic;
        if (materials[i].specular != 0) {
            asset->roughness = 1.0f - avg(materials[i].specular);
        } else {
            asset->roughness = materials[i].roughness;
        }
        if (materials[i].diffuse_texname != "") {
            if (textureAssets.find(materials[i].diffuse_texname) == textureAssets.end()) {
                asset->colorMap = manager.CreateAsset<TextureAsset>(materials[i].diffuse_texname);
                ImportTexture(parentPath + materials[i].diffuse_texname, asset->colorMap);
                textureAssets[materials[i].diffuse_texname] = asset->colorMap;
            } else {
                asset->colorMap = textureAssets[materials[i].diffuse_texname];
            }
        }
        if (materials[i].normal_texname != "") {
            if (textureAssets.find(materials[i].normal_texname) == textureAssets.end()) {
                asset->normalMap = manager.CreateAsset<TextureAsset>(materials[i].normal_texname);
                ImportTexture(parentPath + materials[i].normal_texname, asset->normalMap);
                textureAssets[materials[i].normal_texname] = asset->normalMap;
            } else {
                asset->normalMap = textureAssets[materials[i].normal_texname];
            }
        }
        materialAssets.push_back(asset);
    }

    Ref<SceneAsset> scene = manager.CreateAsset<SceneAsset>(filename);
    Ref<Node> parentNode = manager.CreateObject<Node>(filename);
    scene->Add(parentNode);
    for (size_t i = 0; i < shapes.size(); i++) {
        if (shapes[i].mesh.indices.size() == 0) {
            continue;
        }
        std::unordered_map<MeshAsset::MeshVertex, uint32_t> uniqueVertices{};
        int splittedShapeIndex = 0;
        size_t j = 0;
        size_t lastMaterialId = shapes[i].mesh.material_ids.size() > 0 ? shapes[i].mesh.material_ids[0] : -1;
        Ref<MeshAsset> asset = manager.CreateAsset<MeshAsset>(filename + ":" + shapes[i].name);

        // Calculate bounds for normalization
        glm::vec3 minBounds = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

        // First pass to find bounds
        for (const auto& index : shapes[i].mesh.indices) {
            glm::vec3 position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            minBounds = glm::min(minBounds, position);
            maxBounds = glm::max(maxBounds, position);
        }

        glm::vec3 center = (minBounds + maxBounds) * 0.5f;

        // Second pass to create normalized vertices
        for (const auto& index : shapes[i].mesh.indices) {
            MeshAsset::MeshVertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0] - center.x,
                attrib.vertices[3 * index.vertex_index + 1] - center.y,
                attrib.vertices[3 * index.vertex_index + 2] - center.z
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
                    Node::SetParent(model, parentNode);
                    model->mesh = asset;
                    model->position = center;
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
    Log::Info("Objects: %d", parentNode->children.size());
    return scene->uuid;
}

}