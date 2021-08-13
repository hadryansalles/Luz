#include "Luzpch.hpp"

#include "MeshManager.hpp"

#include <tiny_obj_loader.h>

void MeshManager::Create() {
}

void MeshManager::Destroy() {
    for (MeshResource* mesh : meshes) {
        BufferManager::Destroy(mesh->vertexBuffer);
        BufferManager::Destroy(mesh->indexBuffer);
    }
    meshes.clear();
}

MeshResource* MeshManager::CreateMesh(MeshDesc& desc) {
    MeshResource* mesh = new MeshResource();
    mesh->indexCount = desc.indices.size();
    BufferManager::CreateVertexBuffer(mesh->vertexBuffer, desc.vertices.data(), sizeof(desc.vertices[0]) * desc.vertices.size());
    BufferManager::CreateIndexBuffer(mesh->indexBuffer, desc.indices.data(), sizeof(desc.indices[0]) * desc.indices.size());
    meshes.push_back(mesh);
    return mesh;
}