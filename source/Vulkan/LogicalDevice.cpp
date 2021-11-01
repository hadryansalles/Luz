#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "Luzpch.hpp"

#include "LogicalDevice.hpp"

void LogicalDevice::Create() {
    LUZ_PROFILE_FUNC();
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
    if (supportedFeatures.logicOp)           { features.logicOp           = VK_TRUE; }
    if (supportedFeatures.samplerAnisotropy) { features.samplerAnisotropy = VK_TRUE; }
    if (supportedFeatures.sampleRateShading) { features.sampleRateShading = VK_TRUE; }
    if (supportedFeatures.fillModeNonSolid)  { features.fillModeNonSolid  = VK_TRUE; }
    if (supportedFeatures.wideLines)         { features.wideLines         = VK_TRUE; }
    if (supportedFeatures.depthClamp)        { features.depthClamp        = VK_TRUE; }

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

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.runtimeDescriptorArray = true;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = true;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.pNext = &descriptorIndexingFeatures;

    // specify the required layers to the device 
    if (Instance::IsLayersEnabled()) {
        auto& layers = Instance::GetActiveLayers();
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    auto res = vkCreateDevice(PhysicalDevice::GetVkPhysicalDevice(), &createInfo, nullptr, &device);
    DEBUG_VK(res, "Failed to create logical device!");

    vkGetDeviceQueue(device, PhysicalDevice::GetGraphicsFamily(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, PhysicalDevice::GetPresentFamily(), 0, &presentQueue);

    // command pool
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = PhysicalDevice::GetGraphicsFamily();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        res = vkCreateCommandPool(device, &poolInfo, Instance::GetAllocator(), &commandPool);
        DEBUG_VK(res, "Failed to create command pool!");
    }

    dirty = false;
}

void LogicalDevice::Destroy() {
    vkDestroyCommandPool(device, commandPool, Instance::GetAllocator());
    vkDestroyDevice(device, Instance::GetAllocator());
    device = VK_NULL_HANDLE;
    commandPool = VK_NULL_HANDLE;
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

VkCommandBuffer LogicalDevice::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void LogicalDevice::EndSingleTimeCommands(VkCommandBuffer& commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(LogicalDevice::GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(LogicalDevice::GetGraphicsQueue());

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}