#include "Luzpch.hpp"

#include "MeshManager.hpp"

#include <tiny_obj_loader.h>

void MeshManager::Create() {
    LUZ_PROFILE_FUNC();
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
    descs.clear();
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

void MeshManager::OnImgui() {
    const float leftSpacing = ImGui::GetContentRegionAvailWidth()*1.0f/3.0f;
    if (ImGui::CollapsingHeader("Meshes")) {
        for (int i = 0; i < descs.size(); i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(descs[i]->name.c_str())) {
                ImGui::Text("Path");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("path");
                ImGui::Text(descs[i]->path.string().c_str());
                ImGui::PopID();
                ImGui::Text("Vertices");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("verticesCount");
                ImGui::Text("%u", descs[i]->vertices.size());
                ImGui::PopID();
                ImGui::Text("Indices");
                ImGui::SameLine(leftSpacing);
                ImGui::PushID("indicesCount");
                ImGui::Text("%u", descs[i]->indices.size());
                ImGui::PopID();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}