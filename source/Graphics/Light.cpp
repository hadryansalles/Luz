#include "Luzpch.hpp"

#include "Light.hpp"

void LightManager::PointLightOnImgui(Light* light) {
}

std::string LightManager::GetTypeName(LightType type) {
    if (type == LightType::PointLight) {
        return "Point";
    }
    return "Invalid";
}

void LightManager::OnImgui(Light* light) {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("typeCombo");
        if (ImGui::BeginCombo("", LightManager::GetTypeName(light->type).c_str())) {
            if (ImGui::Selectable("Point", light->type == LightType::PointLight)) {
                light->type = LightType::PointLight;
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        ImGui::Text("Color");
        ImGui::SameLine();
        ImGui::PushID("color");
        ImGui::ColorEdit4("", glm::value_ptr(light->color));
        ImGui::PopID();
        ImGui::Text("Intensity");
        ImGui::SameLine();
        ImGui::PushID("intensity");
        ImGui::DragFloat("", &light->intensity);
        ImGui::PopID();
    }
}
