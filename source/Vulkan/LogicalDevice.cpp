#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "Luzpch.hpp"

#include "LogicalDevice.hpp"

void LogicalDevice::Create() {
    auto instance = Instance::GetVkInstance();

    std::set<uint32_t> uniqueFamilies = {
        PhysicalDevice::GetPresentFamily(),
        PhysicalDevice::GetGraphicsFamily() 
    };

    // priority for each type of queue
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo.queueFamilyIndex = family;
        createInfo.queueCount = 1;
        createInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(createInfo);
    }
    
    auto supportedFeatures = PhysicalDevice::GetFeatures();

    // logical device features
    VkPhysicalDeviceFeatures features{};
    if (supportedFeatures.logicOp)           { features.logicOp = VK_TRUE;           }
    if (supportedFeatures.samplerAnisotropy) { features.samplerAnisotropy = VK_TRUE; }
    if (supportedFeatures.sampleRateShading) { features.sampleRateShading = VK_TRUE; }

    auto requiredExtensions = PhysicalDevice::GetRequiredExtensions();
    auto allExtensions = PhysicalDevice::GetExtensions();
    for (auto req : requiredExtensions) {
        bool available = false;
        for (size_t i = 0; i < allExtensions.size(); i++) {
            if (strcmp(allExtensions[i].extensionName, req) == 0) { 
                available = true; 
                break;
            }
        }
        if(!available) {
            LOG_ERROR("Required extension {0} not available!", req);
        }
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.pEnabledFeatures = &features;

    // specify the required layers to the device 
    if (Instance::IsValidationLayersEnabled()) {
        auto& layers = Instance::GetValidationLayers();
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    auto res = vkCreateDevice(PhysicalDevice::GetVkPhysicalDevice(), &createInfo, nullptr, &device);
    DEBUG_VK(res, "failed to create logical device!");

    vkGetDeviceQueue(device, PhysicalDevice::GetGraphicsFamily(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, PhysicalDevice::GetPresentFamily(), 0, &presentQueue);

    dirty = false;
}

void LogicalDevice::Destroy() {
    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    presentQueue = VK_NULL_HANDLE;
    graphicsQueue = VK_NULL_HANDLE;
}

void LogicalDevice::OnImgui() {
    if (ImGui::CollapsingHeader("Logical Device")) {
        if (ImGui::TreeNode("Active Extensions")) {
            for (auto ext : PhysicalDevice::GetRequiredExtensions()) {
                ImGui::BulletText("%s", ext);
            }
            ImGui::TreePop();
        }
    }
}