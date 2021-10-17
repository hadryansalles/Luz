#include "Luzpch.hpp"

#include "MaterialManager.hpp"
#include "TextureManager.hpp"
#include "SceneManager.hpp"

void MaterialManager::DiffuseOnImgui(Model* model) {
    Material& material = model->material;
    if(ImGui::TreeNodeEx("Diffuse", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Diffuse Color");
        ImGui::SameLine();
        ImGui::PushID("diffuseColor");
        ImGui::ColorEdit4("", glm::value_ptr(material.diffuseColor));
        ImGui::PopID();
        ImGui::Text("Use texture");
        ImGui::SameLine();
        ImGui::PushID("useDiffuseTexture");
        ImGui::Checkbox("", &material.useDiffuseTexture);
        ImGui::PopID();
        if (material.useDiffuseTexture) {
            TextureManager::DrawOnImgui(material.diffuseTexture);
            if (ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* texturePayload = ImGui::AcceptDragDropPayload("texture");
                if (texturePayload) {
                    std::string texturePath((const char*)texturePayload->Data, texturePayload->DataSize);
                    SceneManager::LoadAndSetTexture(model, texturePath);
                    ImGui::EndDragDropTarget();
                }
            }
        }
        ImGui::TreePop();
    }
}

void MaterialManager::SpecularOnImgui(Model* model) {

}

void MaterialManager::OnImgui(Model* model) {
    Material& material = model->material;
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("shaderCombo");
        if (ImGui::BeginCombo("", MaterialManager::GetTypeName(material.type).c_str())) {
            if (ImGui::Selectable("Unlit", material.type == MaterialType::Unlit)) {
                material.type = MaterialType::Unlit;
            }
            if (ImGui::Selectable("Phong", material.type == MaterialType::Phong)) {
                material.type = MaterialType::Phong;
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        DiffuseOnImgui(model);
    }
}
