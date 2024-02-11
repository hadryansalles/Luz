#include "Editor.h"
#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "Camera.hpp"
#include "VulkanWrapper.h"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/ImGuizmo.h>
#include <imgui/IconsFontAwesome5.h>

struct EditorImpl {
    std::vector<Ref<Node>> selectedNodes;
    std::vector<Ref<Node>> copiedNodes;
    bool fullscreen = false;

#define LUZ_SCENE_ICON ICON_FA_GLOBE_AMERICAS
#define LUZ_MESH_ICON ICON_FA_CUBE
#define LUZ_TEXTURE_ICON ICON_FA_IMAGE
#define LUZ_CAMERA_ICON ICON_FA_CAMERA
#define LUZ_NODE_ICON ""
#define LUZ_LIGHT_ICON ICON_FA_LIGHTBULB
#define LUZ_MATERIAL_ICON ICON_FA_PALETTE

    bool assetTypeFilter[int(ObjectType::Count)] = {};
    std::string objectTypeIcon[int(ObjectType::Count)] = {};
    std::string assetNameFilter = "";
    void OnNode(Ref<Node> node);
    void InspectMeshNode(AssetManager& manager, Ref<MeshNode> node);
    void InspectLightNode(AssetManager& manager, Ref<LightNode> node);
    void InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material);
    void OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
    void Select(Ref<Node>& node);
    int FindSelected(Ref<Node>& node);
};

Editor::Editor() {
    impl = new EditorImpl();
    for (int i = 0; i < int(ObjectType::Count); i++) {
        impl->assetTypeFilter[i] = true;
    }
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    const float fontSize = 18.0f;
    io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/roboto.ttf", fontSize);
    ImFontConfig config;
    const float iconSize = fontSize * 2.8f / 3.0f;
    config.MergeMode = true;
    //config.PixelSnapH = true;
    config.GlyphMinAdvanceX = iconSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("assets/fontawesome.otf", iconSize, &config, icon_ranges);
    //io.Fonts->AddFontFromFileTTF("assets/fontawesome-webfont.ttf", iconSize, &config, icon_ranges);

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // todo: find some solution to higher resolution having very small font size
    {
        constexpr auto ColorFromBytes = [](uint8_t r, uint8_t g, uint8_t b) {
            return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
        };

        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        const ImVec4 bgColor           = ColorFromBytes(37, 37, 38);
        const ImVec4 lightBgColor      = ColorFromBytes(82, 82, 85);
        const ImVec4 veryLightBgColor  = ColorFromBytes(90, 90, 95);

        const ImVec4 panelColor        = ColorFromBytes(51, 51, 55);
        const ImVec4 panelHoverColor   = ColorFromBytes(29, 151, 236);
        const ImVec4 panelActiveColor  = ColorFromBytes(0, 119, 200);

        const ImVec4 textColor         = ColorFromBytes(255, 255, 255);
        const ImVec4 textDisabledColor = ColorFromBytes(151, 151, 151);
        const ImVec4 borderColor       = ColorFromBytes(78, 78, 78);

        colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.25f);
        colors[ImGuiCol_Text]                 = textColor;
        colors[ImGuiCol_TextDisabled]         = textDisabledColor;
        colors[ImGuiCol_TextSelectedBg]       = panelActiveColor;
        colors[ImGuiCol_ChildBg]              = bgColor;
        colors[ImGuiCol_PopupBg]              = bgColor;
        colors[ImGuiCol_Border]               = borderColor;
        colors[ImGuiCol_BorderShadow]         = borderColor;
        colors[ImGuiCol_FrameBg]              = panelColor;
        colors[ImGuiCol_FrameBgHovered]       = panelHoverColor;
        colors[ImGuiCol_FrameBgActive]        = panelActiveColor;
        colors[ImGuiCol_TitleBg]              = bgColor;
        colors[ImGuiCol_TitleBgActive]        = bgColor;
        colors[ImGuiCol_TitleBgCollapsed]     = bgColor;
        colors[ImGuiCol_MenuBarBg]            = panelColor;
        colors[ImGuiCol_ScrollbarBg]          = panelColor;
        colors[ImGuiCol_ScrollbarGrab]        = lightBgColor;
        colors[ImGuiCol_ScrollbarGrabHovered] = veryLightBgColor;
        colors[ImGuiCol_ScrollbarGrabActive]  = veryLightBgColor;
        colors[ImGuiCol_CheckMark]            = panelActiveColor;
        colors[ImGuiCol_SliderGrab]           = panelHoverColor;
        colors[ImGuiCol_SliderGrabActive]     = panelActiveColor;
        colors[ImGuiCol_Button]               = panelColor;
        colors[ImGuiCol_ButtonHovered]        = panelHoverColor;
        colors[ImGuiCol_ButtonActive]         = panelHoverColor;
        colors[ImGuiCol_Header]               = panelColor;
        colors[ImGuiCol_HeaderHovered]        = panelHoverColor;
        colors[ImGuiCol_HeaderActive]         = panelActiveColor;
        colors[ImGuiCol_Separator]            = borderColor;
        colors[ImGuiCol_SeparatorHovered]     = borderColor;
        colors[ImGuiCol_SeparatorActive]      = borderColor;
        colors[ImGuiCol_ResizeGrip]           = bgColor;
        colors[ImGuiCol_ResizeGripHovered]    = panelColor;
        colors[ImGuiCol_ResizeGripActive]     = lightBgColor;
        colors[ImGuiCol_PlotLines]            = panelActiveColor;
        colors[ImGuiCol_PlotLinesHovered]     = panelHoverColor;
        colors[ImGuiCol_PlotHistogram]        = panelActiveColor;
        colors[ImGuiCol_PlotHistogramHovered] = panelHoverColor;
        colors[ImGuiCol_DragDropTarget]       = bgColor;
        colors[ImGuiCol_NavHighlight]         = bgColor;
        colors[ImGuiCol_DockingPreview]       = panelActiveColor;
        colors[ImGuiCol_Tab]                  = bgColor;
        colors[ImGuiCol_TabActive]            = panelActiveColor;
        colors[ImGuiCol_TabUnfocused]         = bgColor;
        colors[ImGuiCol_TabUnfocusedActive]   = panelActiveColor;
        colors[ImGuiCol_TabHovered]           = panelHoverColor;

        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 0.0f;
        style.FrameRounding     = 0.0f;
        style.GrabRounding      = 0.0f;
        style.PopupRounding     = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.TabRounding       = 0.0f;
    }
    impl->objectTypeIcon[int(ObjectType::Node)] = LUZ_NODE_ICON;
    impl->objectTypeIcon[int(ObjectType::MeshNode)] = LUZ_MESH_ICON;
    impl->objectTypeIcon[int(ObjectType::MeshAsset)] = LUZ_MESH_ICON;
    impl->objectTypeIcon[int(ObjectType::TextureAsset)] = LUZ_TEXTURE_ICON;
    impl->objectTypeIcon[int(ObjectType::SceneAsset)] = LUZ_SCENE_ICON;
    impl->objectTypeIcon[int(ObjectType::LightNode)] = LUZ_LIGHT_ICON;
    impl->objectTypeIcon[int(ObjectType::MaterialAsset)] = LUZ_MATERIAL_ICON;
}

Editor::~Editor() {
    delete impl;
}

void Editor::BeginFrame() {
    vkw::BeginImGui();
    ImGui::NewFrame();
    //ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
}

ImDrawData* Editor::EndFrame() {
    ImGui::Render();
    return ImGui::GetDrawData();
}

void EditorImpl::OnNode(Ref<Node> node) {
    ImGui::PushID(node->uuid);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.size() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (FindSelected(node) != -1) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string icon = objectTypeIcon[int(node->type)];

    bool open = ImGui::TreeNodeEx((icon + " " + node->name).c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        Select(node);
    }
    if (open) {
        for (auto& child : node->children) {
            OnNode(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void EditorImpl::OnTransform(Camera& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent) {
    // todo: use parent transform to allow World option
    bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
    if (!open) {
        return;
    }
    if (ImGui::Button("Reset")) {
        position = glm::vec3(0);
        rotation = glm::vec3(0);
        scale = glm::vec3(1);
    }
    ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1);
    ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1);
    ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1);
    scale = glm::max(scale, { 0.0001, 0.0001, 0.0001 });

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        currentGizmoOperation = ImGuizmo::SCALE;
    }
    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
        currentGizmoOperation = ImGuizmo::SCALE;
    }

    if (currentGizmoOperation != ImGuizmo::SCALE) {
        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL)) {
            currentGizmoMode = ImGuizmo::LOCAL;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD)) {
            currentGizmoMode = ImGuizmo::WORLD;
        }
    } else {
        currentGizmoMode = ImGuizmo::LOCAL;
    }
    glm::mat4 modelMat = Node::ComposeTransform(position, rotation, scale, parent);
    glm::mat4 guizmoProj(camera.GetProj());
    guizmoProj[1][1] *= -1;
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::Manipulate(glm::value_ptr(camera.GetView()), glm::value_ptr(guizmoProj), currentGizmoOperation, currentGizmoMode, glm::value_ptr(modelMat));
    modelMat = glm::inverse(parent) * modelMat;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMat), glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));
    ImGui::Separator();
}

int EditorImpl::FindSelected(Ref<Node>& node) {
    auto it = std::find_if(selectedNodes.begin(), selectedNodes.end(), [&](const Ref<Node>& other) {
        return node->uuid == other->uuid;
    });
    return it == selectedNodes.end() ? -1 : it - selectedNodes.begin();
}

void EditorImpl::Select(Ref<Node>& node) {
    bool holdingCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    int index = FindSelected(node);
    if (index != -1 && holdingCtrl) {
        selectedNodes.erase(selectedNodes.begin() + index);
    }
    if (index == -1) {
        if (!holdingCtrl) {
            selectedNodes.clear();
        }
        selectedNodes.push_back(node);
    }
}

void Editor::DemoPanel() {
    ImGui::ShowDemoWindow();
}

void Editor::ScenePanel(Ref<SceneAsset>& scene, Camera& camera) {
    if (impl->fullscreen) {
        return;
    }
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Name: %s", scene->name.c_str());
        ImGui::Text("Add");
        ImGui::SameLine();
        if (ImGui::Button("Empty")) {
            
        }
        ImGui::SameLine();
        if (ImGui::Button("Mesh")) {
            auto node = scene->Add<MeshNode>();
            node->name = "New Mesh";
            impl->selectedNodes = { node };
        }
        ImGui::SameLine();
        if (ImGui::Button("Light")) {
            auto newLight = scene->Add<LightNode>();
            newLight->name = "New Light";
            impl->selectedNodes = { newLight };
        }
        if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& node : scene->nodes) {
                impl->OnNode(node);
            }
            bool pressingCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
            if (pressingCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                impl->copiedNodes = impl->selectedNodes;
            }
            if (pressingCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
                for (auto& node : impl->selectedNodes) {
                    scene->Add(Node::Clone(node));
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                for (auto& node : impl->selectedNodes) {
                    scene->DeleteRecursive(node);
                }
                impl->selectedNodes.clear();
            }
        }
        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SeparatorText("Ambient Light");
            ImGui::ColorEdit3("Color", glm::value_ptr(scene->ambientLightColor));
            ImGui::DragFloat("Intesity", &scene->ambientLight, 0.01f, 0.0f, 100.0f);
            ImGui::SeparatorText("Ambient Occlusion");
            ImGui::DragInt("Samples##AO", (int*)&scene->aoSamples, 1, 0, 256);
            ImGui::DragFloat("Min##AO", &scene->aoMin, 0.01f, 0.000f, 10.0f);
            ImGui::DragFloat("Max##AO", &scene->aoMax, 0.1f, 0.000f, 1000.0f);
            ImGui::DragFloat("Power##AO", &scene->aoPower, 0.01f, 0.000f, 100.0f);
            ImGui::SeparatorText("Lights");
            ImGui::DragFloat("Exposure##lights", &scene->exposure, 0.01, 0, 10);
            ImGui::DragInt("Samples##lights", (int*)&scene->lightSamples, 1, 0, 256);
        }
        camera.OnImgui();
    }
    ImGui::End();
}

void Editor::InspectorPanel(AssetManager& assetManager, Camera& camera) {
    bool open = ImGui::Begin("Inspector");
    if (open && impl->selectedNodes.size() > 0) {
        // todo: handle multi selection
        Ref<Node>& selected = impl->selectedNodes.back();
        // todo: fix uuid on imgui
        ImGui::InputInt("UUID", (int*)&selected->uuid, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputText("Name", &selected->name);
        {

        }
        impl->OnTransform(camera, selected->position, selected->rotation, selected->scale, selected->GetParentTransform());
        switch (selected->type) {
            case ObjectType::MeshNode:
                impl->InspectMeshNode(assetManager, std::dynamic_pointer_cast<MeshNode>(selected));
                break;
            case ObjectType::LightNode:
                impl->InspectLightNode(assetManager, std::dynamic_pointer_cast<LightNode>(selected));
                break;
        }
    }
    ImGui::End();
}

void EditorImpl::InspectLightNode(AssetManager& manager, Ref<LightNode> node) {
    ImGui::ColorEdit3("Color", glm::value_ptr(node->color));
    ImGui::DragFloat("Intensity", &node->intensity, 0.01, 0, 1000, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat("Radius", &node->radius, 0.1, 0.0001, 10000);
    ImGui::Checkbox("Shadow", &node->shadows);
}

void EditorImpl::InspectMeshNode(AssetManager& manager, Ref<MeshNode> node) {
    std::string preview = node->mesh ? node->mesh->name : "UNSELECTED";
    if (ImGui::BeginCombo("Mesh", preview.c_str())) {
        for (const auto& mesh : manager.GetAll<MeshAsset>(ObjectType::MeshAsset)) {
            bool selected = node->mesh ? node->mesh->uuid == mesh->uuid : false;
            if (ImGui::Selectable(mesh->name.c_str(), selected)) {
                node->mesh = mesh;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    preview = node->material ? node->material->name : "UNSELECTED";
    if (ImGui::BeginCombo("Material", preview.c_str())) {
        for (const auto& material : manager.GetAll<MaterialAsset>(ObjectType::MaterialAsset)) {
            bool selected = node->material ? node->material->uuid == material->uuid : false;
            if (ImGui::Selectable(material->name.c_str(), selected)) {
                node->material = material;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        if (node->material) {
            node->material = manager.CloneAsset<MaterialAsset>(node->material);
        } else {
            node->material = manager.CreateAsset<MaterialAsset>("New Material");
        }
    }
    if (node->material) {
        InspectMaterial(manager, node->material);
    }
    ImGui::Separator();
}

void EditorImpl::InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material) {
    ImGui::PushID(material->uuid);
    ImGui::InputText("Name", &material->name);
    ImGui::ColorEdit4("Color", glm::value_ptr(material->color));
    ImGui::ColorEdit3("Emission", glm::value_ptr(material->emission));
    ImGui::DragFloat("Roughness", &material->roughness, 0.005, 0, 1);
    ImGui::DragFloat("Metallic", &material->metallic, 0.005, 0, 1);
    ImGui::PopID();
}

void Editor::AssetsPanel(AssetManager& manager) {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }
    ImGui::InputText(ICON_FA_SEARCH " Filter", &impl->assetNameFilter);
    ImGui::Checkbox(LUZ_MESH_ICON " Mesh", &impl->assetTypeFilter[int(ObjectType::MeshAsset)]);
    ImGui::SameLine();
    ImGui::Checkbox(LUZ_SCENE_ICON " Scene", &impl->assetTypeFilter[int(ObjectType::SceneAsset)]);
    ImGui::SameLine();
    ImGui::Checkbox(LUZ_TEXTURE_ICON " Texture", &impl->assetTypeFilter[int(ObjectType::TextureAsset)]);
    ImGui::SameLine();
    ImGui::Checkbox(LUZ_MATERIAL_ICON " Material", &impl->assetTypeFilter[int(ObjectType::MaterialAsset)]);
    ImGui::SameLine();
    if (ImGui::Button("All/None")) {
        for (int i = 0; i < int(ObjectType::Count); i++) {
            impl->assetTypeFilter[i] = !impl->assetTypeFilter[int(ObjectType::Count)-1];
        }
    }
    for (auto& asset : manager.GetAll()) {
        if (!impl->assetTypeFilter[int(asset->type)]) {
            continue;
        }
        if (impl->assetNameFilter != "" && asset->name.find(impl->assetNameFilter) == std::string::npos) {
            continue;
        }
        ImGui::PushID(asset->uuid);
        if (ImGui::CollapsingHeader((impl->objectTypeIcon[int(asset->type)] + " " + asset->name).c_str())) {
            // todo: check type and inspect
        }
        ImGui::PopID();
    }
    ImGui::End();
}

bool Editor::ViewportPanel(vkw::Image& image, glm::ivec2& newViewportSize) {
    bool hovered = false;
    if (ImGui::Begin("Viewport", 0)) {
        ImGui::BeginChild("##ChildViewport");
        newViewportSize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
        ImGui::Image(image.ImGuiRID(), ImGui::GetWindowSize());
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
        hovered = ImGui::IsWindowHovered() && !ImGuizmo::IsUsing();
        ImGui::EndChild();
    }
    ImGui::End();
    return hovered;
}

void Editor::ProfilerPanel() {
    if (ImGui::Begin("Profiler")) {
        std::map<std::string, float> timeTable;
        vkw::GetTimeStamps(timeTable);
        for (const auto& pair : timeTable) {
            ImGui::Text("%s: %.3f", pair.first.c_str(), pair.second);
        }
    }
    ImGui::End();
}

void Editor::ProfilerPopup() {
    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoNav;
    flags |= ImGuiWindowFlags_NoDecoration;
    flags |= ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("ProfilerPopup", 0, flags)) {
        ImGui::SetWindowPos(ImVec2(0, 0));
        std::map<std::string, float> timeTable;
        vkw::GetTimeStamps(timeTable);
        for (const auto& pair : timeTable) {
            ImGui::Text("%s: %.3f", pair.first.c_str(), pair.second);
        }
    }
    ImGui::End();
}