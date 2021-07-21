#include "Luzpch.hpp"

#include "Scene.hpp"

void Scene::Init() {

}

void Scene::Save() {

}

void Scene::Destroy() {

}

void DirOnImgui(std::filesystem::path path) {
    if (ImGui::TreeNode(path.filename().string().c_str())) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                DirOnImgui(entry.path());
            }
            else {
                ImGui::Text(entry.path().filename().string().c_str());
            }
        }
        ImGui::TreePop();
    }
}

void Scene::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Path");
        ImGui::SameLine(totalWidth*3.0f/5.0);
        ImGui::Text(path.string().c_str());
        if (ImGui::CollapsingHeader("Files")) {
            ImGui::Text("Auto Reload");
            ImGui::SameLine(totalWidth*3.0f/5.0f);
            ImGui::PushID("autoReload");
            ImGui::Checkbox("", &autoReloadFiles);
            ImGui::PopID();
            for (const auto& entry : std::filesystem::directory_iterator(path.parent_path())) {
                if (entry.is_directory()) {
                    DirOnImgui(entry.path());
                }
                else {
                    ImGui::Text(entry.path().filename().string().c_str());
                }
            }
        }
    }
    ImGui::End();
}
