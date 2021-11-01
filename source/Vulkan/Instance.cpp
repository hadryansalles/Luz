#include "Luzpch.hpp"

#include "Instance.hpp"
#include "Window.hpp"

#include <imgui/imgui_stdlib.h>

// vulkan debug callbacks
namespace {

// DebugUtilsMessenger utility functions
VkResult CreateDebugUtilsMessengerEXT (
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // search for the requested function and return null if cannot find
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr (
        instance, 
        "vkCreateDebugUtilsMessengerEXT"
    );
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT (
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    // search for the requested function and return null if cannot find
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr (
        instance,
        "vkDestroyDebugUtilsMessengerEXT"
    );
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback (
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    // log the message
    // here we can set a minimum severity to log the message
    // if (messageSeverity > VK_DEBUG_UTILS...)
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_ERROR("[Validation Layer] {0}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;
}

}

void Instance::Create() {
    DEBUG_TRACE("Creating instance.");
    static bool firstInit = true;
    if (firstInit) {
        firstInit = false;
        // get all available layers
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        layers.resize(layerCount);
        activeLayers.resize(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

        // get all available extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, 0);
        extensions.resize(extensionCount);
        activeExtensions.resize(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        // get api version
        vkEnumerateInstanceVersion(&apiVersion);

        // active default khronos validation layer
        bool khronosAvailable = false;
        for (size_t i = 0; i < layers.size(); i++) {
            activeLayers[i] = false;
            if (strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0) {
                activeLayers[i] = true;
                khronosAvailable = true;
                break;
            }
        }

        if (enableLayers && !khronosAvailable) { 
            LOG_ERROR("Default validation layer not available!");
        }
    }

    // active vulkan layer
    allocator = nullptr;

    // get the name of all layers if they are enabled
    if (enableLayers) {
        for (size_t i = 0; i < layers.size(); i++) { 
            if (activeLayers[i]) {
                activeLayersNames.push_back(layers[i].layerName);
            }
        }
    }

    // optional data, provides useful info to the driver
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = applicationName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = engineName.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // tells which global extensions and validations to use
    // (they apply to the entire program)
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // get all extensions required by glfw
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    auto requiredExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // include the extensions required by us
    if (enableLayers) {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // set to active all extensions that we enabled
    for (size_t i = 0; i < requiredExtensions.size(); i++) {
        for (size_t j = 0; j < extensions.size(); j++) {
            if (strcmp(requiredExtensions[i], extensions[j].extensionName) == 0) {
                activeExtensions[j] = true;
                break;
            }
        }
    }

    // get the name of all extensions that we enabled
    activeExtensionsNames.clear();
    for (size_t i = 0; i < extensions.size(); i++) {
        if (activeExtensions[i]) {
            activeExtensionsNames.push_back(extensions[i].extensionName);
        }
    }

    // get and set all required extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(activeExtensionsNames.size());
    createInfo.ppEnabledExtensionNames = activeExtensionsNames.data();

    // which validation layers we need
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(activeLayersNames.size());
        createInfo.ppEnabledLayerNames = activeLayersNames.data();

        // we need to set up a separate logger just for the instance creation/destruction
        // because our "default" logger is created after
        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        // createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // almost all vulkan functions return VkResult, either VK_SUCCESS or an error code
    auto res = vkCreateInstance(&createInfo, allocator, &instance); 
    DEBUG_VK(res, "Failed to create Vulkan instance!");

    DEBUG_TRACE("Created instance.");
    
    if (enableLayers) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
        PopulateDebugMessengerCreateInfo(messengerInfo);

        res = CreateDebugUtilsMessengerEXT(instance, &messengerInfo, allocator, &debugMessenger);
        DEBUG_VK(res, "Failed to set up debug messenger!");
        DEBUG_TRACE("Created debug messenger.");
    }

    res = glfwCreateWindowSurface(instance, Window::GetGLFWwindow(), allocator, &surface);
    DEBUG_VK(res, "Failed to create window surface!");
    DEBUG_TRACE("Created surface.");

    LOG_INFO("Created VulkanInstance.");
    dirty = false;
}

void Instance::Destroy() {
    activeLayersNames.clear();
    activeExtensionsNames.clear();
    if (debugMessenger) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, allocator);
        DEBUG_TRACE("Destroyed debug messenger.");
        debugMessenger = nullptr;
    }
    vkDestroySurfaceKHR(instance, surface, allocator);
    DEBUG_TRACE("Destroyed surface.");
    vkDestroyInstance(instance, allocator);
    DEBUG_TRACE("Destroyed instance.");
    LOG_INFO("Destroyed VulkanInstance");
}

void Instance::OnImgui() {
    if (ImGui::CollapsingHeader("Instance")) {
        float totalWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::Text("API Version");
        ImGui::SameLine(totalWidth/2);
        ImGui::PushID("apiVersion");
        ImGui::Text("%d", apiVersion);
        ImGui::PopID();
        ImGui::Text("Application");
        ImGui::SameLine(totalWidth/2);
        ImGui::PushID("appName");
        ImGui::InputText("", &applicationName);
        ImGui::PopID();
        ImGui::Text("Engine");
        ImGui::SameLine(totalWidth/2);
        ImGui::PushID("engineName");
        ImGui::InputText("", &engineName);
        ImGui::PopID();
        ImGui::Text("Enable Validation Layers");
        ImGui::SameLine(totalWidth/2);
        ImGui::PushID("enableValidation");
        ImGui::Checkbox("", &enableLayers);
        ImGui::PopID();
        ImGui::Separator();
        if (ImGui::TreeNode("Layers")) {
            for (size_t i = 0; i < layers.size(); i++) {
                ImGui::PushID(i);
                bool active = activeLayers[i];
                ImGui::Checkbox("", &active);
                activeLayers[i] = active;
                ImGui::SameLine();
                const auto& layer = layers[i];
                if (ImGui::TreeNode(layer.layerName)) {
                    // description
                    ImGui::Dummy(ImVec2(5.0f, .0f));
                    ImGui::SameLine();
                    ImGui::Text("Description: %s", layer.description);
                    // spec version
                    ImGui::Dummy(ImVec2(5.0f, .0f));
                    ImGui::SameLine();
                    ImGui::Text("Spec Version: %d", layer.specVersion);
                    // impl version
                    ImGui::Dummy(ImVec2(5.0f, .0f));
                    ImGui::SameLine();
                    ImGui::Text("Implementation Version: %d", layer.implementationVersion);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        if (ImGui::TreeNode("Extensions")) {
            for (size_t i = 0; i < extensions.size(); i++) {
                ImGui::PushID(i);
                bool active = activeExtensions[i];
                ImGui::Checkbox("", &active);
                activeExtensions[i] = active;
                ImGui::PopID();
                ImGui::SameLine();
                const auto& ext = extensions[i];
                if (ImGui::TreeNode(ext.extensionName)) {
                    ImGui::Dummy(ImVec2(5.0f, .0f));
                    ImGui::SameLine();
                    ImGui::Text("Spec Version: %d", ext.specVersion);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        if (ImGui::Button("Apply Changes")) {
            dirty = true;
        }
    }
    if (ImGui::IsKeyPressed(GLFW_KEY_I)) {
        dirty = true;
    }
}