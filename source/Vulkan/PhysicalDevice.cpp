#include "Luzpch.hpp" 

#include "PhysicalDevice.hpp"
#include "Instance.hpp"

void PhysicalDevice::Create() {
    auto instance = Instance::GetVkInstance();
    // get all devices with Vulkan support
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    ASSERT(count != 0, "no GPUs with Vulkan support!");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    // initialize all physical devices properties
    for (const auto& device : devices) {
        PhysicalDevice newDevice;
        newDevice.vkDevice = device;

        // get all available extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        newDevice.extensions.resize(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, newDevice.extensions.data());

        // get all available families
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        newDevice.families.resize(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, newDevice.families.data());

        // select the family for each type of queue that we want
        uint32_t i = 0;
        for (const auto& family : newDevice.families) {
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                newDevice.graphicsFamily = i;
            }

            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Instance::GetVkSurface(), &present);
            if (present) {
                newDevice.presentFamily = i;
            }
            i++;

            if (newDevice.presentFamily != -1 && newDevice.graphicsFamily != -1) {
                break;
            }
        }

        // get max number of samples
        vkGetPhysicalDeviceProperties(device, &newDevice.properties);
        vkGetPhysicalDeviceMemoryProperties(device, &newDevice.memoryProperties);

        VkSampleCountFlags counts = newDevice.properties.limits.framebufferColorSampleCounts;
        counts &= newDevice.properties.limits.framebufferDepthSampleCounts;

        newDevice.maxSamples = VK_SAMPLE_COUNT_1_BIT;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_64_BIT; }
        else if (counts & VK_SAMPLE_COUNT_32_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_32_BIT; }
        else if (counts & VK_SAMPLE_COUNT_16_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_16_BIT; }
        else if (counts & VK_SAMPLE_COUNT_8_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_8_BIT; }
        else if (counts & VK_SAMPLE_COUNT_4_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_4_BIT; }
        else if (counts & VK_SAMPLE_COUNT_2_BIT) { newDevice.maxSamples = VK_SAMPLE_COUNT_2_BIT; }

        // get all supported features
        vkGetPhysicalDeviceFeatures(device, &newDevice.features);

        // check if this device is suitable
        bool suitable = true;

        // check if all required extensions are available
        std::set<std::string> required(requiredExtensions.begin(), requiredExtensions.end());
        for (const auto& extension : newDevice.extensions) {
            required.erase(extension.extensionName);
        }
        suitable &= required.empty();

        // check if all required queues are supported
        suitable &= newDevice.graphicsFamily != -1;
        suitable &= newDevice.presentFamily != -1;

        newDevice.suitable = suitable;
        allDevices.emplace_back(newDevice);
    }
   
    UpdateDevice();

    // get surface related stuff for all devices
    OnSurfaceUpdate();

    isDirty = false;
}

void PhysicalDevice::Destroy() {
    allDevices.clear();
    device = nullptr;
}

void PhysicalDevice::OnImgui() {
    float totalWidth = ImGui::GetContentRegionAvailWidth();
    if (ImGui::CollapsingHeader("Physical Device")) {
        for (size_t i = 0; i < allDevices.size(); i++) {
            auto d = allDevices[i];
            ImGui::PushID(d.properties.deviceID);
            if (ImGui::RadioButton("", i == index) && i != index) {
                index = i;
                isDirty = true;
            }
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNode(d.properties.deviceName)) {
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("API Version: %d", d.properties.apiVersion);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Driver Version: %d", d.properties.driverVersion);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Max Samples: %d", d.maxSamples);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                auto minExt = d.capabilities.minImageExtent;
                ImGui::Text("Min extent: {%d, %d}", minExt.width, minExt.height);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                auto maxExt = d.capabilities.maxImageExtent;
                ImGui::Text("Max extent: {%d, %d}", maxExt.width, maxExt.height);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Min image count: %d", d.capabilities.minImageCount);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Max image count: %d", d.capabilities.maxImageCount);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Graphics Family: %d", d.graphicsFamily);
                ImGui::Dummy(ImVec2(5.0f, .0f));
                ImGui::SameLine();
                ImGui::Text("Present Family: %d", d.presentFamily);
                ImGui::Separator();
                if (ImGui::TreeNode("Extensions")) {
                    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY;
                    flags |= ImGuiTableFlags_RowBg;
                    flags |= ImGuiTableFlags_BordersOuter;
                    flags |= ImGuiTableFlags_BordersV;
                    flags |= ImGuiTableFlags_Resizable;
                    ImVec2 maxSize = ImVec2(0.0f, 200.0f);
                    if (ImGui::BeginTable("extTable", 2, flags, maxSize)) {
                        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Spec Version", ImGuiTableColumnFlags_None);
                        ImGui::TableHeadersRow();
                        for (auto& ext : d.extensions) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", ext.extensionName);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", ext.specVersion);
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                ImGui::Separator();
                if (ImGui::TreeNode("Families")) {
                    ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
                    // flags |= ImGuiTableFlags_ScrollX;
                    flags |= ImGuiTableFlags_BordersOuter;
                    flags |= ImGuiTableFlags_BordersV;
                    if (ImGui::BeginTable("famTable", 8, flags)) {
                        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Graphics", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Compute", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Transfer", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Sparse", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Protected", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Min Granularity", ImGuiTableColumnFlags_None);
                        ImGui::TableHeadersRow();
                        for (size_t j = 0; j < d.families.size(); j++) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%zu", j);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", d.families[j].queueCount);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%c", (d.families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)? 'X' : ' ');
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%c", (d.families[j].queueFlags & VK_QUEUE_COMPUTE_BIT)? 'X' : ' ');
                            ImGui::TableSetColumnIndex(4);
                            ImGui::Text("%c", (d.families[j].queueFlags & VK_QUEUE_TRANSFER_BIT)? 'X' : ' ');
                            ImGui::TableSetColumnIndex(5);
                            ImGui::Text("%c", (d.families[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)? 'X' : ' ');
                            ImGui::TableSetColumnIndex(6);
                            ImGui::Text("%c", (d.families[j].queueFlags & VK_QUEUE_PROTECTED_BIT)? 'X' : ' ');
                            ImGui::TableSetColumnIndex(7);
                            auto min = d.families[j].minImageTransferGranularity;
                            ImGui::Text("%d, %d, %d", min.width, min.height, min.depth);
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                ImGui::Separator();
                if (ImGui::TreeNode("Present Modes")) {
                    ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
                    // flags |= ImGuiTableFlags_ScrollX;
                    flags |= ImGuiTableFlags_BordersOuter;
                    flags |= ImGuiTableFlags_BordersV;
                    if (ImGui::BeginTable("presentModes", 1, flags)) {
                        for (const auto& p : d.presentModes) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            if (p == VK_PRESENT_MODE_FIFO_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_FIFO_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_FIFO_RELAXED_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_IMMEDIATE_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_MAILBOX_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_MAILBOX_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_MAX_ENUM_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_MAX_ENUM_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR");
                            }
                            else if (p == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) {
                                ImGui::Text("VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR");
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
        }
    }
}

void PhysicalDevice::OnSurfaceUpdate() {
    auto surface = Instance::GetVkSurface();

    // get surface related stuff for all physical devices
    for (auto& d : allDevices) {
        auto vkDevice = d.vkDevice;
        // get capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDevice, surface, &d.capabilities);

        // get surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vkDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            d.surfaceFormats.clear();
            d.surfaceFormats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(vkDevice, surface, &formatCount, d.surfaceFormats.data());
        }

        // get present modes
        uint32_t modeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vkDevice, surface, &modeCount, nullptr);
        if (modeCount != 0) {
            d.presentModes.clear();
            d.presentModes.resize(modeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(vkDevice, surface, &modeCount, d.presentModes.data());
        }

        // set this device as not suitable if no surface formats or present modes available
        d.suitable &= (modeCount > 0);
        d.suitable &= (formatCount > 0);
    }

    if (!device->suitable) {
        LOG_WARN("selected device invalidated after surface update. Trying to find a suitable device.");
        UpdateDevice();
    }
}

void PhysicalDevice::UpdateDevice() {
    if (index == -1 || !allDevices[index].suitable) {
        // select the first suitable device to use
        for (size_t i = 0; i < allDevices.size(); i++) {
            if (allDevices[i].suitable) {
                index = i;
            }
        }
    }
    device = &(allDevices[index]);
    return;

    ASSERT(false, "can't find suitable device. Aborting.");
}

uint32_t PhysicalDevice::FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < device->memoryProperties.memoryTypeCount; i++) {
        if (type & (1 << i)) {
            if ((device->memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
    }

    ASSERT(false, "failed to find suitable memory type");
}

bool PhysicalDevice::SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(PhysicalDevice::GetVkPhysicalDevice(), format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return true;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
        return true;
    }

    return false;
}
