#include "Luzpch.hpp"

#include "Mesh.hpp"

#include <tiny_obj_loader.h>

MeshResource* Mesh::Load(std::filesystem::path path) {
    MeshResource* res = new MeshResource;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::unordered_map<MeshVertex, uint32_t> uniqueVertices{};

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str())) {
        throw std::runtime_error(warn + err);
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
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

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(res->vertices.size());
                res->vertices.push_back(vertex);
            }

            res->indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void Mesh::CreateBuffers(MeshResource* res) {

}