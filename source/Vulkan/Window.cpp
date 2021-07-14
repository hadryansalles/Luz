#include "Luzpch.hpp"

#include "Window.hpp"

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

void Window::Create() {
    // initializing glfw
    glfwInit();

    // glfw uses OpenGL context by default
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    monitors = glfwGetMonitors(&monitorCount);
    // ASSERT(monitorIndex < monitorCount, "Invalid monitorIndex inside Window creation!");
    auto monitor = monitors[monitorIndex];
    auto monitorMode = glfwGetVideoMode(monitor);

    // creating window
    switch (mode) {
    case Mode::Windowed:
        window = glfwCreateWindow(width, height, name, nullptr, nullptr);
        glfwSetWindowPos(window, posX, posY);
        if (maximized) {
            glfwMaximizeWindow(window);
        }
        glfwSetWindowAttrib(window, GLFW_MAXIMIZED, maximized);
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable);
        glfwSetWindowAttrib(window, GLFW_DECORATED, decorated);
        break;
    case Mode::WindowedFullScreen:
        glfwWindowHint(GLFW_RED_BITS, monitorMode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, monitorMode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, monitorMode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, monitorMode->refreshRate);
        window = glfwCreateWindow(monitorMode->width, monitorMode->height, name, monitor, nullptr);
        break;
    case Mode::FullScreen:
        window = glfwCreateWindow(width, height, name, monitor, nullptr);
        break;
    }
    glfwSetFramebufferSizeCallback(window, Window::FramebufferResizeCallback);
    glfwSetScrollCallback(window, Window::ScrollCallback);
    glfwSetWindowMaximizeCallback(window, Window::WindowMaximizeCallback);

    framebufferResized = false;
    dirty = false;

    // LOG_INFO("Created Window");
}

void Window::Destroy() {
    glfwGetWindowPos(window, &posX, &posY);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::Update() {
    deltaScroll = 0;
    auto newTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(newTime - lastTime).count();
    deltaTime /= 1000.0f;
    lastTime = newTime;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    deltaMousePos = mousePos - glm::vec2(x, y);
    mousePos = glm::vec2(x, y);
    glfwPollEvents();
}

void Window::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
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
                        mode = (Window::Mode)i;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        if (mode != Mode::Windowed) {
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
            if (mode == Mode::FullScreen || (mode == Mode::Windowed && maximized == false)) {
                ImGui::Text("Resolution");
                ImGui::SameLine(totalWidth / 2.0f);
                ImGui::SetNextItemWidth(totalWidth / 4.0f);
                ImGui::PushID("width");
                ImGui::InputInt("", &width, 1);
                ImGui::PopID();
                ImGui::SameLine(3*totalWidth / 4.0f);
                ImGui::SetNextItemWidth(totalWidth / 4.0f);
                ImGui::PushID("height");
                ImGui::InputInt("", &height, 1);
                ImGui::PopID();
            }
        }
        // windowed only
        {
            if (mode == Mode::Windowed) {
                // maximized
                {
                    ImGui::Text("Maximized");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("maximized");
                    ImGui::Checkbox("", &maximized);
                    ImGui::PopID();
                }
                // decorated
                {
                    ImGui::Text("Decorated");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("decorated");
                    ImGui::Checkbox("", &decorated);
                    ImGui::PopID();
                }
                // resizable
                {
                    ImGui::Text("Resizable");
                    ImGui::SameLine(totalWidth / 2.0f);
                    ImGui::SetNextItemWidth(totalWidth / 2.0f);
                    ImGui::PushID("resizable");
                    ImGui::Checkbox("", &resizable);
                    ImGui::PopID();
                }
            }
        }
        if (ImGui::Button("Recreate")) {
            dirty = true;
        }
    }
}

void Window::UpdateFramebufferSize() {
    framebufferResized = false;
    glfwGetFramebufferSize(window, &width, &height);
}