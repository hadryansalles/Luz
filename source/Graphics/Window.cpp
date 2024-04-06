#include "Luzpch.hpp"

#include "Window.hpp"
#include <imgui/imgui.h>

void Window::ScrollCallback(GLFWwindow* window, double x, double y) {
    Window::scroll += y;
    Window::deltaScroll += y;
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Window::width = width;
    Window::height = height;
    Window::framebufferResized = true;
}

void Window::WindowMaximizeCallback(GLFWwindow* window, int maximize) {
    maximized = maximize;
}

void Window::WindowChangePosCallback(GLFWwindow* window, int x, int y) {
    Window::posX = x;
    Window::posY = y;
}

void Window::WindowDropCallback(GLFWwindow* window, int count, const char* paths[]) {
    for (int i = 0; i < count; i++) {
        pathsDrop.push_back(paths[i]);
    }
}

void Window::Create() {
    LUZ_PROFILE_FUNC();
    // initializing glfw
    glfwInit();

    // glfw uses OpenGL context by default
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    monitors = glfwGetMonitors(&monitorCount);

    glfwGetVideoModes(monitors[monitorIndex], &videoModeIndex);
    videoModeIndex -= 1;

    window = glfwCreateWindow(width, height, name, nullptr, nullptr);
    glfwSetWindowPos(window, posX, posY);

    glfwSetFramebufferSizeCallback(window, Window::FramebufferResizeCallback);
    glfwSetScrollCallback(window, Window::ScrollCallback);
    glfwSetWindowMaximizeCallback(window, Window::WindowMaximizeCallback);
    glfwSetWindowPosCallback(window, Window::WindowChangePosCallback);
    glfwSetDropCallback(window, Window::WindowDropCallback);

    dirty = false;
    Window::ApplyChanges();
}

void Window::ApplyChanges() {
    monitors = glfwGetMonitors(&monitorCount);
    // ASSERT(monitorIndex < monitorCount, "Invalid monitorIndex inside Window creation!");
    auto monitor = monitors[monitorIndex];
    auto monitorMode = glfwGetVideoMode(monitor);

    int modesCount;
    const GLFWvidmode* videoModes = glfwGetVideoModes(monitors[monitorIndex], &modesCount);
    if (videoModeIndex >= modesCount) {
        videoModeIndex = modesCount - 1;
    }

    // creating window
    switch (mode) {
    case WindowMode::Windowed:
        posY = std::max(posY, 31);
        glfwSetWindowMonitor(window, nullptr, posX, posY, width, height, GLFW_DONT_CARE);
        if (maximized) {
            glfwMaximizeWindow(window);
        }
        glfwSetWindowAttrib(window, GLFW_MAXIMIZED, maximized);
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable);
        glfwSetWindowAttrib(window, GLFW_DECORATED, decorated);
        break;
    case WindowMode::WindowedFullScreen:
        glfwWindowHint(GLFW_RED_BITS, monitorMode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, monitorMode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, monitorMode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, monitorMode->refreshRate);
        glfwSetWindowMonitor(window, monitor, 0, 0, monitorMode->width, monitorMode->height, monitorMode->refreshRate);
        break;
    case WindowMode::FullScreen:
        GLFWvidmode videoMode = videoModes[videoModeIndex];
        glfwSetWindowMonitor(window, monitor, 0, 0, videoMode.width, videoMode.height, videoMode.refreshRate);
        break;
    }

    framebufferResized = false;
    dirty = false;
}

void Window::Destroy() {
    glfwGetWindowPos(window, &posX, &posY);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::Update() {
    LUZ_PROFILE_NAMED("WindowUpdate");
    for (int i = 0; i < GLFW_KEY_LAST + 1; i++) {
        lastKeyState[i] = glfwGetKey(window, i);
    }
    deltaScroll = 0;
    auto newTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(newTime - lastTime).count();
    deltaTime /= 1000.0f;
    lastTime = newTime;
    std::this_thread::sleep_for(std::chrono::milliseconds(30 - (int)deltaTime));
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    deltaMousePos = mousePos - glm::vec2(x, y);
    mousePos = glm::vec2(x, y);
    glfwPollEvents();
}

std::string VideoModeText(GLFWvidmode mode) {
    return std::to_string(mode.width) + "x" + std::to_string(mode.height) + " " + std::to_string(mode.refreshRate) + " Hz";
}

void Window::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvail().x;
    if (ImGui::CollapsingHeader("Window")) {
        // mode
        {
            const char* modeNames[] = { "Windowed", "Windowed FullScreen", "FullScreen"};
            ImGui::Text("Mode");
            ImGui::SameLine(totalWidth / 2.0f);
            ImGui::SetNextItemWidth(totalWidth / 2.0f);
            ImGui::PushID("modeCombo");
            if (ImGui::BeginCombo("", modeNames[(int)mode])) {
                for (int i = 0; i < 3; i++) {
                    bool selected = (int)mode == i;
                    if (ImGui::Selectable(modeNames[i], selected)) {
                        mode = (WindowMode)i;
                        dirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        if (mode != WindowMode::Windowed) {
            // monitor
            {
                ImGui::Text("Monitor");
                ImGui::SameLine(totalWidth / 2.0f);
                ImGui::SetNextItemWidth(totalWidth / 2.0f);
                ImGui::PushID("monitorCombo");
                if (ImGui::BeginCombo("", glfwGetMonitorName(monitors[monitorIndex]))) {
                    for (int i = 0; i < monitorCount; i++) {
                        bool selected = monitorIndex == i;
                        ImGui::PushID(i);
                        if (ImGui::Selectable(glfwGetMonitorName(monitors[i]), selected)) {
                            monitorIndex = i;
                            dirty = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
        }
        // resolution
        {
            if (mode == WindowMode::FullScreen) {
                ImGui::Text("Resolution");
                ImGui::SameLine(totalWidth / 2.0f);
                ImGui::SetNextItemWidth(totalWidth / 4.0f);
                ImGui::PushID("monitorRes");
                int modesCount;
                const GLFWvidmode* videoModes = glfwGetVideoModes(monitors[monitorIndex], &modesCount);
                GLFWvidmode currMode = videoModes[videoModeIndex];
                std::string modeText = VideoModeText(currMode);
                if (ImGui::BeginCombo("", modeText.c_str())) {
                    for (int i = 0; i < modesCount; i++) {
                        bool selected = videoModeIndex == i;
                        currMode = videoModes[i];
                        ImGui::PushID(i);
                        modeText = VideoModeText(currMode);
                        if (ImGui::Selectable(modeText.c_str(), selected)) {
                            videoModeIndex = i;
                            dirty = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
        }
        // windowed only
        {
            if (mode == WindowMode::Windowed) {
                // maximized
                {
                    ImGui::Text("Maximized");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("maximized");
                    if (ImGui::Checkbox("", &maximized)) {
                        dirty = true;
                    }
                    ImGui::PopID();
                }
                // decorated
                {
                    ImGui::Text("Decorated");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("decorated");
                    if (ImGui::Checkbox("", &decorated)) {
                        dirty = true;
                    }
                    ImGui::PopID();
                }
                // resizable
                {
                    ImGui::Text("Resizable");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("resizable");
                    if (ImGui::Checkbox("", &resizable)) {
                        dirty = true;
                    }
                    ImGui::PopID();
                }
            }
        }
    }
}

void Window::UpdateFramebufferSize() {
    framebufferResized = false;
    glfwGetFramebufferSize(window, &width, &height);
}

bool Window::IsKeyPressed(uint16_t keyCode) {
    return lastKeyState[keyCode] && !glfwGetKey(window, keyCode);
}