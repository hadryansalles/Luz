#include "Luzpch.hpp"

#include "MeshManager.hpp"

#include <tiny_obj_loader.h>

void MeshManager::Create() {
    DEBUG_ASSERT(meshes.size() == descs.size(), "Number of mesh descs is different than number of meshes!");
    for (size_t i = 0; i < meshes.size(); i++) {
        SetupMesh(descs[i], meshes[i]);
    }
}

void MeshManager::Destroy() {
    for (MeshResource* mesh : meshes) {
        BufferManager::Destroy(mesh->vertexBuffer);
        BufferManager::Destroy(mesh->indexBuffer);
    }
}

void MeshManager::Finish() {
    for (MeshDesc* desc : descs) {
        delete desc;
    }
    for (MeshResource* mesh : meshes) {
        delete mesh;
    }
    meshes.clear();
}

void MeshManager::SetupMesh(MeshDesc* desc, MeshResource* res) {
    res->indexCount = desc->indices.size();
    BufferManager::CreateVertexBuffer(res->vertexBuffer, desc->vertices.data(), sizeof(desc->vertices[0]) * desc->vertices.size());
    BufferManager::CreateIndexBuffer(res->indexBuffer, desc->indices.data(), sizeof(desc->indices[0]) * desc->indices.size());
}

MeshResource* MeshManager::CreateMesh(MeshDesc* desc) {
    MeshResource* mesh = new MeshResource();
    MeshManager::SetupMesh(desc, mesh);
    meshes.push_back(mesh);
    descs.push_back(desc);
    return mesh;
}