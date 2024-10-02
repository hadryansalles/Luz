#include "Editor.h"
#include "Luzpch.hpp"

#include "GPUScene.hpp"
#include "AssetManager.hpp"
#include "VulkanWrapper.h"
#include "Window.hpp"
#include "DebugDraw.h"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/ImGuizmo.h>
#include <imgui/IconsFontAwesome5.h>

struct EditorImpl {
    std::vector<Ref<Node>> selectedNodes;
    std::vector<Ref<Node>> copiedNodes;
    bool profilerPopup = true;

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
    void InspectLightNode(AssetManager& manager, Ref<LightNode> node, GPUScene& gpuScene);
    void InspectMaterial(AssetManager& manager, Ref<MaterialAsset> material);
    void OnTransform(const Ref<CameraNode>& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent = glm::mat4(1));
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
    const float fontSize = 17.0f;
    io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/roboto.ttf", fontSize);
    ImFontConfig config;
    const float iconSize = fontSize * 2.8f / 3.0f;
    config.MergeMode = true;
    //config.PixelSnapH = true;
    config.GlyphMinAdvanceX = iconSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("assets/fontawesome.otf", iconSize, &config, icon_ranges);

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

        colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.65f);
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
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
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
        flags |= ImGuiTreeNodeFlags_Bullet;
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

void EditorImpl::OnTransform(const Ref<CameraNode>& camera, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale, glm::mat4 parent) {
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
    glm::mat4 guizmoProj(camera->GetProj());
    guizmoProj[1][1] *= -1;
    ImGuiIO& io = ImGui::GetIO();
    glm::mat4 camView = camera->GetView();
    ImGuizmo::Manipulate(glm::value_ptr(camView), glm::value_ptr(guizmoProj), currentGizmoOperation, currentGizmoMode, glm::value_ptr(modelMat));
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

void Editor::Select(AssetManager& manager, const std::vector<Ref<Node>>& nodes) {
    impl->selectedNodes = nodes;
}

void Editor::DemoPanel() {
    ImGui::ShowDemoWindow();
}

void Editor::ScenePanel(Ref<SceneAsset>& scene) {
    if (ImGui::Begin("Scene")) {
        ImGui::Text("Name: %s", scene->name.c_str());
        ImGui::Text("Add");
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
            ImGui::DragFloat("Exposure", &scene->exposure, 0.01, 0, 10);

            ImGui::SeparatorText("Ambient Light");
            ImGui::ColorEdit3("Color", glm::value_ptr(scene->ambientLightColor));
            ImGui::DragFloat("Intesity", &scene->ambientLight, 0.01f, 0.0f, 100.0f);

            ImGui::SeparatorText("Ambient Occlusion");
            bool aoEnable = scene->aoSamples >= 0;
            if (ImGui::Checkbox("Enable##AO", &aoEnable)) {
                scene->aoSamples *= -1;
            }
            if (aoEnable) {
                ImGui::DragInt("Samples##AO", (int*)&scene->aoSamples, 1, 0, 256);
            }
            ImGui::DragFloat("Min##AO", &scene->aoMin, 0.01f, 0.000f, 10.0f);
            ImGui::DragFloat("Max##AO", &scene->aoMax, 0.1f, 0.000f, 1000.0f);

            ImGui::SeparatorText("Lights");
            ImGui::DragInt("Samples##lights", (int*)&scene->lightSamples, 1, 0, 256);

            ImGui::SeparatorText("Shadows");
            if (ImGui::BeginCombo("Type###Shadow", ShadowTypeNames[(int)scene->shadowType].c_str())) {
                for (int i = 0; i < ShadowType::ShadowTypeCount; i++) {
                    bool selected = scene->shadowType == i;
                    if (ImGui::Selectable(ShadowTypeNames[i].c_str(), &selected)) {
                        scene->shadowType = (ShadowType)i;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SeparatorText("TAA");
            ImGui::Checkbox("Enable##TAA", &scene->taaEnabled);
            ImGui::Checkbox("Reconstruction##TAA", &scene->taaReconstruct);
            ImGui::Checkbox("Jitter##TAA", &scene->mainCamera->useJitter);
        }
        // todo: scene camera prameters, speed, etc
    }
    ImGui::End();
}

void Editor::InspectorPanel(AssetManager& assetManager, const Ref<CameraNode>& camera, GPUScene& gpuScene) {
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
                impl->InspectLightNode(assetManager, std::dynamic_pointer_cast<LightNode>(selected), gpuScene);
                break;
        }
    }
    ImGui::End();
}

void EditorImpl::InspectLightNode(AssetManager& manager, Ref<LightNode> node, GPUScene& gpuScene) {
    ImGui::ColorEdit3("Color", glm::value_ptr(node->color));
    if (ImGui::BeginCombo("Type", LightNode::typeNames[node->lightType])) {
        for (int i = 0; i < LightNode::LightType::LightTypeCount; i++) {
            bool selected = node->lightType == i;
            if (ImGui::Selectable(LightNode::typeNames[i], &selected)) {
                node->lightType = (LightNode::LightType)i;
            }
        }
        ImGui::EndCombo();
    }
    if (node->lightType == LightNode::LightType::Spot) {
        ImGui::DragFloat("Inner Angle", &node->innerAngle, 0.05, 0.0, 90.0);
        ImGui::DragFloat("Outer Angle", &node->outerAngle, 0.05, 0.0, 90.0);
    }
    ImGui::DragFloat("Intensity", &node->intensity, 0.1, 0, 1000, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat("Radius", &node->radius, 0.1, 0.0001, 10000);
    if (ImGui::CollapsingHeader("Shadow Map")) {
        ImGui::DragFloat("Range##Shadow", &node->shadowMapRange, 0.01f);
        ImGui::DragFloat("Far##Shadow", &node->shadowMapFar, 0.1f);
        if (gpuScene.GetShadowMap(node->uuid).readable) {
            auto& img = gpuScene.GetShadowMap(node->uuid).img;
            if (node->lightType == LightNode::LightType::Point) {
                for (int i = 0; i < 6; i++) {
                    ImGui::Image(img.ImGuiRID(i), ImVec2(400, 400*img.height/img.width));
                }
            } else {
                ImGui::Image(img.ImGuiRID(), ImVec2(400, 400*img.height/img.width));
            }
        }
    }
    if (ImGui::CollapsingHeader("Volumetric Light")) {
        if (ImGui::BeginCombo("Volumetric", LightNode::volumetricTypeNames[node->volumetricType])) {
            for (int i = 0; i < LightNode::VolumetricType::VolumetricLightCount; i++) {
                bool selected = node->volumetricType == i;
                if (ImGui::Selectable(LightNode::volumetricTypeNames[i], &selected)) {
                    node->volumetricType = (LightNode::VolumetricType)i;
                }
            }
            ImGui::EndCombo();
        }
        if (node->volumetricType == LightNode::VolumetricType::ScreenSpace) {
            ImGui::DragFloat("Absorption##Volumetric", &node->volumetricScreenSpaceParams.absorption, 0.01f, 0.0f, 1.0f);
            ImGui::DragInt("Samples##Volumetric", &node->volumetricScreenSpaceParams.samples, 1, 0, 256);
        } else if (node->volumetricType == LightNode::VolumetricType::ShadowMap) {
            ImGui::DragFloat("Weight##Volumetric", &node->volumetricShadowMapParams.weight);
            ImGui::DragFloat("Absorption##Volumetric", &node->volumetricShadowMapParams.absorption);
            ImGui::DragFloat("Density##Volumetric", &node->volumetricShadowMapParams.density);
            ImGui::DragInt("Samples##Volumetric", &node->volumetricShadowMapParams.samples);
        }
    }
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    if (ImGui::Begin("Viewport")) {
        ImGui::BeginChild("##ChildViewport");
        newViewportSize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
        ImGui::Image(image.ImGuiRID(), ImGui::GetWindowSize());
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
        hovered = ImGui::IsWindowHovered() && !ImGuizmo::IsUsing();
        ImGui::EndChild();
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
    return hovered;
}

void Editor::ProfilerPanel() {
}

void Editor::DebugDrawPanel() {
    if (!ImGui::Begin("Debug Draw")) {
        ImGui::End();
        return;
    }
    auto data = DebugDraw::Get();
    for (auto& line : data) {
        if (ImGui::CollapsingHeader(line.name.c_str())) {
            bool changed = ImGui::Checkbox("Hide", &line.config.hide);
            changed |= ImGui::Checkbox("Update", &line.config.update);
            changed |= ImGui::Checkbox("Depth", &line.config.depthAware);
            changed |= ImGui::DragFloat("Thickness", &line.config.thickness, 0.01, 0, 10);
            changed |= ImGui::ColorPicker4("Color", glm::value_ptr(line.config.color));
            if (changed) {
                DebugDraw::Config(line.name, line.config, false);
            }
        }
    }
    ImGui::End();
}

void Editor::ProfilerPopup() {
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
        impl->profilerPopup = !impl->profilerPopup;
    }
    if (!impl->profilerPopup) {
        return;
    }
    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoNav;
    flags |= ImGuiWindowFlags_NoDecoration;
    flags |= ImGuiWindowFlags_NoInputs;
    flags |= ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("ProfilerPopup", 0, flags)) {
        std::map<std::string, float> timeTable;
        vkw::GetTimeStamps(timeTable);
        ImGui::SeparatorText("GPU (ms)");
        for (const auto& pair : timeTable) {
            ImGui::Text("%s: %.3f", pair.first.c_str(), pair.second);
        }
        ImGui::SeparatorText("CPU (ms)");
        const auto& cpuTimes = TimeScope::GetCPUTimes();
        for (const auto& pair : cpuTimes) {
            ImGui::Text("%s: %.3f", pair.first.c_str(), pair.second);
        }
        ImGui::Separator();
        ImGui::Text("F5  - Reload Shaders");
        ImGui::Text("F7  - Change Output Mode");
        ImGui::Text("F10 - Profiler");
        ImGui::Text("F11 - Fullscreen");
        ImVec2 maxPos = ImGui::GetMainViewport()->Size;
        ImVec2 panelSize = ImGui::GetWindowSize();
        ImGui::SetWindowPos({ maxPos.x - panelSize.x, 0});
    }
    ImGui::End();
}