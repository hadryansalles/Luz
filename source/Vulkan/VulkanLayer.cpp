#include "Luzpch.hpp"

#include "VulkanLayer.h"
#include "GraphicsPipelineManager.hpp"

#include "common.h"
#include "imgui/imgui_impl_vulkan.h"

namespace vkw {

static Context _ctx;

struct Resource {
    std::string name;
    virtual ~Resource() {};
};

struct BufferResource : Resource {
    VkBuffer buffer;
    VkDeviceMemory memory;

    virtual ~BufferResource() {
        vkDestroyBuffer(_ctx.device, buffer, _ctx.allocator);
        vkFreeMemory(_ctx.device, memory, _ctx.allocator);
    }
};

struct ImageResource : Resource {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    bool fromSwapchain = false;

    virtual ~ImageResource() {
        if (!fromSwapchain) {
            vkDestroyImageView(_ctx.device, view, _ctx.allocator);
            vkDestroyImage(_ctx.device, image, _ctx.allocator);
            vkFreeMemory(_ctx.device, memory, _ctx.allocator);
        }
    }
};

void Init(GLFWwindow* window, uint32_t width, uint32_t height) {
    _ctx.CreateInstance(window);
    _ctx.CreatePhysicalDevice();
    _ctx.CreateDevice();
    _ctx.CreateSurfaceFormats();
    _ctx.CreateSwapChain(width, height);
}

void OnSurfaceUpdate(uint32_t width, uint32_t height) {
    _ctx.DestroySwapChain();
    _ctx.CreateSurfaceFormats();
    _ctx.CreateSwapChain(width, height);
}

void Destroy() {
    _ctx.DestroySwapChain();
    _ctx.DestroyDevice();
    _ctx.DestroyInstance();
}

Buffer CreateBuffer(uint32_t size, BufferUsageFlags usage, MemoryFlags memory, const std::string& name) {
    if (usage & BufferUsage::Vertex) {
        usage |= BufferUsage::TransferDst;
    }

    if (usage & BufferUsage::Index) {
        usage |= BufferUsage::TransferDst;
    }

    if (usage & BufferUsage::Storage) {
        usage |= BufferUsage::Address;
        size += size % _ctx.physicalProperties.limits.minStorageBufferOffsetAlignment;
    }

    if (usage & BufferUsage::AccelerationStructureInput) {
        usage |= BufferUsage::Address;
        usage |= BufferUsage::TransferDst;
    }

    std::shared_ptr<BufferResource> res = std::make_shared<BufferResource>();
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = (VkBufferUsageFlagBits)usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto result = vkCreateBuffer(_ctx.device, &bufferInfo, _ctx.allocator, &res->buffer);
    DEBUG_VK(result, "Failed to create buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(_ctx.device, res->buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = _ctx.FindMemoryType(memReq.memoryTypeBits, (VkMemoryPropertyFlags)memory);

    result = vkAllocateMemory(_ctx.device, &allocInfo, _ctx.allocator, &res->memory);
    DEBUG_VK(result, "Failed to allocate buffer memory!");

    vkBindBufferMemory(_ctx.device, res->buffer, res->memory, 0);
    res->name = name;

    Buffer buffer = {
        .resource = res,
        .size = size,
        .usage = usage,
        .memory = memory,
        .rid = 0
    };

    if (usage & BufferUsage::Storage) {
        buffer.rid = _ctx.nextBufferRID++;
        VkDescriptorBufferInfo descriptorInfo = {};
        VkWriteDescriptorSet write = {};
        descriptorInfo.buffer = res->buffer;
        descriptorInfo.offset = 0;
        descriptorInfo.range = size;
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
        write.dstBinding = LUZ_BINDING_BUFFER;
        write.dstArrayElement = buffer.rid;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &descriptorInfo;
        vkUpdateDescriptorSets(_ctx.device, 1, &write, 0, nullptr);
    }

    return buffer;
}

Image CreateImage(const ImageDesc& desc) {
    auto device = _ctx.device;
    auto allocator = _ctx.allocator;

    std::shared_ptr<ImageResource> res = std::make_shared<ImageResource>();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = (VkFormat)desc.format;
    // tiling defines how the texels lay in memory
    // optimal tiling is implementation dependent for more efficient memory access
    // and linear makes the texels lay in row-major order, possibly with padding on each row
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // not usable by the GPU, the first transition will discard the texels
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = (VkImageUsageFlags)desc.usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    auto result = vkCreateImage(device, &imageInfo, allocator, &res->image);
    DEBUG_VK(result, "Failed to create image!");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, res->image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = _ctx.FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(device, &allocInfo, allocator, &res->memory);
    DEBUG_VK(result, "Failed to allocate image memory!");

    vkBindImageMemory(device, res->image, res->memory, 0);

    AspectFlags aspect = Aspect::Color;
    if (desc.format == Format::D24_unorm_S8_uint || desc.format == Format::D32_sfloat) {
        aspect = Aspect::Depth;
    }
    if (desc.format == Format::D24_unorm_S8_uint) {
        aspect |= Aspect::Stencil;
    }

    res->name = desc.name;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = (VkFormat)desc.format;
    viewInfo.subresourceRange.aspectMask = (VkImageAspectFlags)aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device, &viewInfo, allocator, &res->view);
    DEBUG_VK(result, "Failed to create image view!");

    Image image = {
        .resource = res,
        .width = desc.width,
        .height = desc.height,
        .usage = desc.usage,
        .format = desc.format,
        .layout = Layout::Undefined,
        .aspect = aspect,
        .rid = _ctx.nextImageRID++,
    };

    if (desc.usage & ImageUsage::Sampled) {
        Layout::ImageLayout newLayout = Layout::ShaderRead;
        if (aspect == (Aspect::Depth | Aspect::Stencil)) {
            newLayout = Layout::DepthStencilRead;
        } else if (aspect == Aspect::Depth) {
            newLayout = Layout::DepthRead;
        }
        image.imguiRID = ImGui_ImplVulkan_AddTexture(_ctx.genericSampler, res->view, (VkImageLayout)newLayout);

        VkDescriptorImageInfo descriptorInfo = {
            .sampler = _ctx.genericSampler,
            .imageView = res->view,
            .imageLayout = (VkImageLayout)newLayout,
        };
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
        write.dstBinding = LUZ_BINDING_TEXTURE;
        write.dstArrayElement = image.rid;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &descriptorInfo;
        vkUpdateDescriptorSets(_ctx.device, 1, &write, 0, nullptr);
    }

    VkDebugUtilsObjectNameInfoEXT name = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
    .objectHandle = (uint64_t)(VkImage)res->image,
    .pObjectName = desc.name.c_str(),
    };
    _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &name);
    std::string strName = desc.name + "View";
    name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = (uint64_t)(VkImageView)res->view,
        .pObjectName = strName.c_str(),
    };
    _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &name);

    return image;
}

VkImageView Image::GetView() {
    return resource->view;
}

VkImage Image::GetImage() {
    return resource->image;
}

void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset) {
    _ctx.CmdCopy(dst, data, size, dstOfsset);
}

void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset) {
    _ctx.CmdCopy(dst, src, size, dstOffset, srcOffset);
}

void CmdCopy(Image& dst, void* data, uint32_t size) {
    _ctx.CmdCopy(dst, data, size); 
}

void CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset) {
    _ctx.CmdCopy(dst, src, size, srcOffset); 
}

void CmdBarrier(Image& img, Layout::ImageLayout layout) {
    _ctx.CmdBarrier(img, layout);
}

void CmdBarrier() {
    _ctx.CmdBarrier();
}

void BeginCommandBuffer(Queue queue) {
    ASSERT(_ctx.currentQueue == Queue::Count, "Already recording a command buffer");

    _ctx.currentQueue = queue;
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkWaitForFences(_ctx.device, 1, &cmd.fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_ctx.device, 1, &cmd.fence);

    Context::InternalQueue& iqueue = _ctx.queues[queue];
    vkResetCommandPool(_ctx.device, iqueue.commands[_ctx.currentImageIndex].pool, 0);
    iqueue.commands[_ctx.currentImageIndex].stagingOffset = 0;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(_ctx.queues[queue].commands[_ctx.currentImageIndex].buffer, &beginInfo);
}

uint64_t EndCommandBuffer() {
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkEndCommandBuffer(cmd.buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd.buffer;

    auto res = vkQueueSubmit(_ctx.queues[_ctx.currentQueue].queue, 1, &submitInfo, cmd.fence);
    DEBUG_VK(res, "Failed to submit command buffer");
    _ctx.currentQueue = vkw::Queue::Count;
    return 0;
}

void WaitQueue(Queue queue) {
    // todo: wait on fence
    auto res = vkQueueWaitIdle(_ctx.queues[queue].queue);
    DEBUG_VK(res, "Failed to wait idle command buffer");
}

//void CmdBeginPresent(/*swapchain*/);
//void CmdEndPresent();
    // BeginCommandBuffer(Queue::Transfer);
    // CmdCopy(buffer, data, size, dstOffset);
    // EndCommandBuffer(Queue::Transfer);
    // WaitQueue(Queue::Transfer);

VkBuffer Buffer::GetBuffer() {
    if (resource) {
        return resource->buffer;
    }
    return nullptr;
}

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

void Context::CreateInstance(GLFWwindow* glfwWindow) {
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
        instanceExtensions.resize(extensionCount);
        activeExtensions.resize(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, instanceExtensions.data());

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

        if (enableValidationLayers && !khronosAvailable) { 
            LOG_ERROR("Default validation layer not available!");
        }
    }

    // active vulkan layer
    allocator = nullptr;

    // get the name of all layers if they are enabled
    if (enableValidationLayers) {
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
    appInfo.apiVersion = VK_API_VERSION_1_3;

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
    if (enableValidationLayers) {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // set to active all extensions that we enabled
    for (size_t i = 0; i < requiredExtensions.size(); i++) {
        for (size_t j = 0; j < instanceExtensions.size(); j++) {
            if (strcmp(requiredExtensions[i], instanceExtensions[j].extensionName) == 0) {
                activeExtensions[j] = true;
                break;
            }
        }
    }

    // get the name of all extensions that we enabled
    activeExtensionsNames.clear();
    for (size_t i = 0; i < instanceExtensions.size(); i++) {
        if (activeExtensions[i]) {
            activeExtensionsNames.push_back(instanceExtensions[i].extensionName);
        }
    }

    // get and set all required extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(activeExtensionsNames.size());
    createInfo.ppEnabledExtensionNames = activeExtensionsNames.data();

    // which validation layers we need
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
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
    
    if (enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
        PopulateDebugMessengerCreateInfo(messengerInfo);

        res = CreateDebugUtilsMessengerEXT(instance, &messengerInfo, allocator, &debugMessenger);
        DEBUG_VK(res, "Failed to set up debug messenger!");
        DEBUG_TRACE("Created debug messenger.");
    }

    res = glfwCreateWindowSurface(instance, glfwWindow, allocator, &surface);
    DEBUG_VK(res, "Failed to create window surface!");
    DEBUG_TRACE("Created surface.");

    LOG_INFO("Created VulkanInstance.");
}

void Context::DestroyInstance() {
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

void Context::CreatePhysicalDevice() {
    LUZ_PROFILE_FUNC();
    // get all devices with Vulkan support
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    ASSERT(count != 0, "no GPUs with Vulkan support!");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    // get first physical device that matches all requirements
    for (const auto& device : devices) {
        // get all available extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        availableExtensions.resize(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // get all available families
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        availableFamilies.resize(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, availableFamilies.data());

        int computeFamily = -1;
        int transferFamily = -1;
        int graphicsFamily = -1;

        // select the family for each type of queue that we want
        for (int i = 0; i < familyCount; i++) {
            auto& family = availableFamilies[i];
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT && graphicsFamily == -1) {
                VkBool32 present = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, this->surface, &present);
                if (present) {
                    graphicsFamily = i;
                }
                continue;
            }

            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && computeFamily == -1) {
                computeFamily = i;
                continue;
            }

            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT && transferFamily == -1) {
                transferFamily = i;
                continue;
            }
        }

        queues[Queue::Graphics].family = graphicsFamily;
        queues[Queue::Compute].family = computeFamily == -1 ? graphicsFamily : computeFamily;
        queues[Queue::Transfer].family = transferFamily == -1 ? graphicsFamily : transferFamily;

        // get max number of samples
        vkGetPhysicalDeviceFeatures(device, &physicalFeatures);
        vkGetPhysicalDeviceProperties(device, &physicalProperties);
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        VkSampleCountFlags counts = physicalProperties.limits.framebufferColorSampleCounts;
        counts &= physicalProperties.limits.framebufferDepthSampleCounts;

        maxSamples = VK_SAMPLE_COUNT_1_BIT;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { maxSamples = VK_SAMPLE_COUNT_64_BIT; }
        else if (counts & VK_SAMPLE_COUNT_32_BIT) { maxSamples = VK_SAMPLE_COUNT_32_BIT; }
        else if (counts & VK_SAMPLE_COUNT_16_BIT) { maxSamples = VK_SAMPLE_COUNT_16_BIT; }
        else if (counts & VK_SAMPLE_COUNT_8_BIT) { maxSamples = VK_SAMPLE_COUNT_8_BIT; }
        else if (counts & VK_SAMPLE_COUNT_4_BIT) { maxSamples = VK_SAMPLE_COUNT_4_BIT; }
        else if (counts & VK_SAMPLE_COUNT_2_BIT) { maxSamples = VK_SAMPLE_COUNT_2_BIT; }

        // check if all required extensions are available
        std::set<std::string> required(requiredExtensions.begin(), requiredExtensions.end());
        for (const auto& extension : availableExtensions) {
            required.erase(std::string(extension.extensionName));
        }

        // check if all required queues are supported
        bool suitable = required.empty();
        suitable &= graphicsFamily != -1;

        if (suitable) {
            physicalDevice = device;
            break;
        }
    }

}

void Context::CreateDevice() {
    LUZ_PROFILE_FUNC();
    std::set<uint32_t> uniqueFamilies;
    for (int q = 0; q < Queue::Count; q++) {
        uniqueFamilies.emplace(queues[q].family);
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

    auto supportedFeatures = physicalFeatures;

    // logical device features
    VkPhysicalDeviceFeatures features{};
    if (supportedFeatures.logicOp)           { features.logicOp           = VK_TRUE; }
    if (supportedFeatures.samplerAnisotropy) { features.samplerAnisotropy = VK_TRUE; }
    if (supportedFeatures.sampleRateShading) { features.sampleRateShading = VK_TRUE; }
    if (supportedFeatures.fillModeNonSolid)  { features.fillModeNonSolid  = VK_TRUE; }
    if (supportedFeatures.wideLines)         { features.wideLines         = VK_TRUE; }
    if (supportedFeatures.depthClamp)        { features.depthClamp        = VK_TRUE; }

    auto requiredExtensions = vkw::ctx().requiredExtensions;
    auto allExtensions = vkw::ctx().availableExtensions;
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
    descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddresFeatures{};
    bufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT;
    bufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddresFeatures.pNext = &descriptorIndexingFeatures;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &bufferDeviceAddresFeatures;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
    accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_TRUE;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = VK_TRUE;
    rayQueryFeatures.pNext = &accelerationStructureFeatures;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext = &rayQueryFeatures;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features{};
    sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2Features.synchronization2 = VK_TRUE;
    sync2Features.pNext = &dynamicRenderingFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.pNext = &sync2Features;

    // specify the required layers to the device 
    if (vkw::ctx().enableValidationLayers) {
        auto& layers = activeLayersNames;
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    auto res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    DEBUG_VK(res, "Failed to create logical device!");

    for (int q = 0; q < Queue::Count; q++) {
        vkGetDeviceQueue(device, queues[q].family, 0, &queues[q].queue);
    }

    genericSampler = CreateSampler(1.0);
    vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
}

void Context::DestroyDevice() {
    vkDestroySampler(device, genericSampler, allocator);
    vkDestroyDevice(device, allocator);
    device = VK_NULL_HANDLE;
}

void Context::CreateSurfaceFormats() {
    auto surface = vkw::ctx().surface;

    // get capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    // get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        availableSurfaceFormats.clear();
        availableSurfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, availableSurfaceFormats.data());
    }

    // get present modes
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
    if (modeCount != 0) {
        availablePresentModes.clear();
        availablePresentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, availablePresentModes.data());
    }

    // set this device as not suitable if no surface formats or present modes available
    bool suitable = (modeCount > 0);
    suitable &= (formatCount > 0);

    if (!suitable) {
        LOG_ERROR("selected device invalidated after surface update.");
    }
}

void Context::CreateSwapChain(uint32_t width, uint32_t height) {
    LUZ_PROFILE_FUNC();

    if (numSamples > maxSamples) {
        numSamples = maxSamples;
    }

    // create swapchain
    {
        const auto& capabilities = surfaceCapabilities;
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(availableSurfaceFormats);
        colorFormat = surfaceFormat.format;
        colorSpace = surfaceFormat.colorSpace;
        presentMode = ChoosePresentMode(availablePresentModes);
        swapChainExtent = ChooseExtent(capabilities, width, height);

        // acquire additional images to prevent waiting for driver's internal operations
        uint32_t imageCount = framesInFlight + additionalImages;
        
        if (imageCount < capabilities.minImageCount) {
            LOG_WARN("Querying less images than the necessary!");
            imageCount = capabilities.minImageCount;
        }

        // prevent exceeding the max image count
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            LOG_WARN("Querying more images than supported. imageCoun set to maxImageCount.");
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vkw::ctx().surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = swapChainExtent;
        // amount of layers each image consist of
        // it's 1 unless we are developing some 3D stereoscopic app
        createInfo.imageArrayLayers = 1;
        // if we want to render to a separate image first to perform post-processing
        // we should change this image usage
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // don't support different graphics and present family
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // here we could specify a transformation to be applied on the images of the swap chain
        createInfo.preTransform = capabilities.currentTransform;

        // here we could blend the images with other windows in the window system
        // to ignore this blending we set OPAQUE
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        // here we clip pixels behind our window
        createInfo.clipped = true;

        // here we specify a handle to when the swapchain become invalid
        // possible causes are changing settings or resizing window
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        auto res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
        DEBUG_VK(res, "Failed to create swap chain!");

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImageResources.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImageResources.data());
    }

    // create image views
    swapChainViews.resize(swapChainImageResources.size());
    swapChainImages.resize(swapChainImageResources.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImageResources[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        auto res = vkCreateImageView(device, &viewInfo, allocator, &swapChainViews[i]);
        DEBUG_VK(res, "Failed to create SwapChain image view!");

        swapChainImages[i].resource = std::make_shared<ImageResource>();
        swapChainImages[i].resource->fromSwapchain = true;
        swapChainImages[i].resource->image = swapChainImageResources[i];
        swapChainImages[i].resource->view = swapChainViews[i];
        swapChainImages[i].layout = Layout::Undefined;
        swapChainImages[i].width = width;
        swapChainImages[i].height = height;
        swapChainImages[i].aspect = Aspect::Color;
    }

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        std::string strName = ("SwapChain Image " + std::to_string(i));
        VkDebugUtilsObjectNameInfoEXT name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
            .objectHandle = (uint64_t)(VkImage)swapChainImageResources[i],
            .pObjectName = strName.c_str(),
        };
        _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &name);
        strName = ("SwapChain View " + std::to_string(i));
        name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = (uint64_t)(VkImageView)swapChainViews[i],
            .pObjectName = strName.c_str(),
        };
        _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &name);
    }

    // synchronization objects
    {
        imageAvailableSemaphores.resize(framesInFlight);
        renderFinishedSemaphores.resize(framesInFlight);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (size_t i = 0; i < framesInFlight; i++) {
            auto res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
            DEBUG_VK(res, "Failed to create semaphore!");
            res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
            DEBUG_VK(res, "Failed to create semaphore!");
        }
    }

    LOG_INFO("Create Swapchain");
    swapChainCurrentFrame = 0;
    currentImageIndex = 0;
    swapChainDirty = false;

    // command pool
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = 0;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        for (int q = 0; q < Queue::Count; q++) {
            InternalQueue& queue = queues[q];
            poolInfo.queueFamilyIndex = queue.family;
            queue.commands.resize(framesInFlight);
            for (int i = 0; i < framesInFlight; i++) {
                auto res = vkCreateCommandPool(device, &poolInfo, allocator, &queue.commands[i].pool);
                DEBUG_VK(res, "Failed to create command pool!");

                allocInfo.commandPool = queue.commands[i].pool;
                res = vkAllocateCommandBuffers(device, &allocInfo, &queue.commands[i].buffer);
                DEBUG_VK(res, "Failed to allocate command buffer!");

                queue.commands[i].staging = CreateBuffer(stagingBufferSize, BufferUsage::TransferSrc, Memory::CPU, "StagingBuffer" + std::to_string(q) + "_" + std::to_string(i));
                vkMapMemory(device, queue.commands[i].staging.resource->memory, 0, stagingBufferSize, 0, (void**)&queue.commands[i].stagingCpu);

                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                vkCreateFence(device, &fenceInfo, allocator, &queue.commands[i].fence);
            }
        }
    }
}

void Context::DestroySwapChain() {
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroyImageView(device, swapChainViews[i], allocator);
    }

    for (size_t i = 0; i < framesInFlight; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], allocator);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], allocator);
    }

    vkDestroySwapchainKHR(device, swapChain, allocator);

    for (int q = 0; q < Queue::Count; q++) {
        for (int i = 0; i < framesInFlight; i++) {
            vkDestroyCommandPool(device, queues[q].commands[i].pool, allocator);
            queues[q].commands[i].staging = {};
            queues[q].commands[i].stagingCpu = nullptr;
            vkDestroyFence(device, queues[q].commands[i].fence, allocator);
        }
    }

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    swapChainViews.clear();
    swapChainImages.clear();
    swapChain = VK_NULL_HANDLE;
}

uint32_t Context::Acquire() {
    LUZ_PROFILE_FUNC();

    uint32_t imageIndex;
    auto res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[swapChainCurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        swapChainDirty = true;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        DEBUG_VK(res, "Failed to acquire swap chain image!");
    }

    currentImageIndex = imageIndex;
    return imageIndex;
}

void Context::SubmitAndPresent(uint32_t imageIndex) {
    LUZ_PROFILE_FUNC();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[swapChainCurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(queues[currentQueue].commands[imageIndex].buffer);

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[swapChainCurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    auto& cmd = _ctx.GetCurrentCommandResources();
    vkEndCommandBuffer(cmd.buffer);

    auto res = vkQueueSubmit(_ctx.queues[_ctx.currentQueue].queue, 1, &submitInfo, cmd.fence);
    DEBUG_VK(res, "Failed to submit command buffer");

    res = vkQueuePresentKHR(queues[currentQueue].queue, &presentInfo);
    currentQueue = Queue::Count;

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        swapChainDirty = true;
        return;
    }
    else if (res != VK_SUCCESS) {
        DEBUG_VK(res, "Failed to present swap chain image!");
    }

    swapChainCurrentFrame = (swapChainCurrentFrame + 1) % framesInFlight;
}

uint32_t Context::FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if (type & (1 << i)) {
            if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
    }

    ASSERT(false, "failed to find suitable memory type");
}


bool Context::SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return true;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
        return true;
    }

    return false;
}

VkSurfaceFormatKHR Context::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == colorFormat
            && availableFormat.colorSpace == colorSpace) {
            return availableFormat;
        }
    }
    LOG_WARN("Preferred surface format not available!");
    return formats[0];
}

VkPresentModeKHR Context::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
    for (const auto& mode : presentModes) {
        if (mode == presentMode) {
            return mode;
        }
    }
    LOG_WARN("Preferred present mode not available!");
    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Context::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { width, height };

        actualExtent.width = std::max (
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width)
        );
        actualExtent.height = std::max (
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height)
        );

        return actualExtent;
    }
}

Context& ctx() {
    return _ctx;
}

void Context::CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset) {
    CommandResources& cmd = queues[currentQueue].commands[currentImageIndex];
    if (stagingBufferSize - cmd.stagingOffset < size) {
        LOG_ERROR("not enough size in staging buffer to copy");
        // todo: allocate additional buffer
        return;
    }
    memcpy(cmd.stagingCpu + cmd.stagingOffset, data, size);
    CmdCopy(dst, cmd.staging, size, dstOfsset, cmd.stagingOffset);
    cmd.stagingOffset += size;
}

void Context::CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset) {
    CommandResources& cmd = queues[currentQueue].commands[currentImageIndex];
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd.buffer, src.resource->buffer, dst.resource->buffer, 1, &copyRegion);
}

void Context::CmdCopy(Image& dst, void* data, uint32_t size) {
    CommandResources& cmd = queues[currentQueue].commands[currentImageIndex];
    if (stagingBufferSize - cmd.stagingOffset < size) {
        LOG_ERROR("not enough size in staging buffer to copy");
        // todo: allocate additional buffer
        return;
    }
    memcpy(cmd.stagingCpu + cmd.stagingOffset, data, size);
    CmdCopy(dst, cmd.staging, size, cmd.stagingOffset);
    cmd.stagingOffset += size;
}

void Context::CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset) {
    CommandResources& cmd = queues[currentQueue].commands[currentImageIndex];
    VkBufferImageCopy region{};
    region.bufferOffset = srcOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    ASSERT(!(dst.aspect & Aspect::Depth || dst.aspect & Aspect::Stencil), "CmdCopy don't support depth/stencil images");
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { dst.width, dst.height, 1 };
    vkCmdCopyBufferToImage(cmd.buffer, src.resource->buffer, dst.resource->image, (VkImageLayout)dst.layout, 1, &region);
}

void Context::CmdBarrier(Image& img, Layout::ImageLayout layout) {
    CommandResources& cmd = queues[currentQueue].commands[currentImageIndex];
    VkImageSubresourceRange range = {};
    range.aspectMask = (VkImageAspectFlags)img.aspect;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;;

    VkAccessFlags srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
    VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = (VkImageLayout)img.layout;
    barrier.newLayout = (VkImageLayout)layout;
    barrier.image = img.resource->image;
    barrier.subresourceRange = range;
    vkCmdPipelineBarrier(cmd.buffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    img.layout = layout;
}

void Context::CmdBarrier() {
    VkMemoryBarrier2 barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
    };
    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(GetCurrentCommandBuffer(), &dependency);
}

VkSampler Context::CreateSampler(f32 maxLod) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    samplerInfo.anisotropyEnable = false;

    // what color to return when clamp is active in addressing mode
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    // if comparison is enabled, texels will be compared to a value an the result 
    // is used in filtering operations, can be used in PCF on shadow maps
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = maxLod;

    VkSampler sampler = VK_NULL_HANDLE;
    auto vkRes = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    DEBUG_VK(vkRes, "Failed to create texture sampler!");

    return sampler;
}

}