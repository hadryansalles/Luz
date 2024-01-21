#include "Luzpch.hpp"

#include "VulkanLayer.h"
#include "BufferManager.hpp"

namespace vkw {

static Context _ctx;

struct Resource {
    uint32_t rid;
    std::string name;
    virtual ~Resource()
    {};
};

struct BufferResource : Resource {
    VkBuffer buffer;
    VkDeviceMemory memory;

    virtual ~BufferResource() {
        vkDestroyBuffer(ctx().device, buffer, ctx().allocator);
        vkFreeMemory(ctx().device, memory, ctx().allocator);
    }
};

void Init(GLFWwindow* window, uint32_t width, uint32_t height) {
    ctx().CreateInstance(window);
    ctx().CreatePhysicalDevice();
    ctx().CreateDevice();
    ctx().CreateSurfaceFormats();
    ctx().CreateSwapChain(width, height);
    // ctx().device = vkw::ctx().device;
    // ctx().allocator = ctx().allocator;
    // ctx().memoryProperties = ctx().memoryProperties;
}

void OnSurfaceUpdate(uint32_t width, uint32_t height) {
    ctx().DestroySwapChain();
    ctx().CreateSurfaceFormats();
    ctx().CreateSwapChain(width, height);
}

void Destroy() {
    ctx().DestroySwapChain();
    ctx().DestroyDevice();
    ctx().DestroyInstance();
}

Buffer CreateBuffer(uint32_t size, UsageFlags usage, MemoryFlags memory, const std::string& name) {

    if (usage & Usage::Vertex) {
        usage |= Usage::TransferDst;
    }

    if (usage & Usage::Index) {
        usage |= Usage::TransferDst;
    }

    if (usage & Usage::AccelerationStructureBuildInputReadOnly) {
        usage |= Usage::Address;
        usage |= Usage::TransferDst;
    }

    std::shared_ptr<BufferResource> res = std::make_shared<BufferResource>();
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = (VkBufferUsageFlagBits)usage;
    // only from the graphics queue
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto result = vkCreateBuffer(ctx().device, &bufferInfo, ctx().allocator, &res->buffer);
    DEBUG_VK(result, "Failed to create buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(ctx().device, res->buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = ctx().FindMemoryType(memReq.memoryTypeBits, (VkMemoryPropertyFlags)memory);

    result = vkAllocateMemory(ctx().device, &allocInfo, ctx().allocator, &res->memory);
    DEBUG_VK(result, "Failed to allocate buffer memory!");

    vkBindBufferMemory(ctx().device, res->buffer, res->memory, 0);
    res->name = name;

    return {
        .resource = res,
        .size = size,
        .usage = usage,
        .memory = memory,
    };
}

void Buffer::SetBuffer(VkBuffer vkBuffer, VkDeviceMemory vkMemory) {
    resource = std::make_shared<BufferResource>();
    resource->buffer = vkBuffer;
    resource->memory = vkMemory;
}

void CmdCopy(Buffer& dest, void* data, uint32_t size, uint32_t dstOfsset) {
    _ctx.CmdCopy(dest, data, size, dstOfsset);
}

void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset) {
    _ctx.CmdCopy(dst, src, size, dstOffset, srcOffset);
}

void BeginCommandBuffer(Queue queue) {
    Context::InternalQueue& iqueue = _ctx.queues[queue];
    vkResetCommandPool(_ctx.device, iqueue.commands[_ctx.currentImageIndex].pool, 0);
    iqueue.commands[_ctx.currentImageIndex].stagingOffset = 0;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _ctx.currentQueue = queue;
    vkBeginCommandBuffer(_ctx.queues[queue].commands[_ctx.currentImageIndex].buffer, &beginInfo);
}

uint64_t EndCommandBuffer(Queue queue) {
    VkCommandBuffer cmdBuffer = _ctx.queues[queue].commands[_ctx.currentImageIndex].buffer;
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    auto res = vkQueueSubmit(_ctx.queues[queue].queue, 1, &submitInfo, VK_NULL_HANDLE);
    DEBUG_VK(res, "Failed to submit command buffer");
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

uint32_t Buffer::StorageID() {
    return 0;
}

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
    if (enableValidationLayers) {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    requiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

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
    graphicsQueue = queues[Queue::Graphics].queue;

    // command pool
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkw::ctx().graphicsFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        res = vkCreateCommandPool(device, &poolInfo, allocator, &commandPool);
        DEBUG_VK(res, "Failed to create command pool!");

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
                res = vkCreateCommandPool(device, &poolInfo, allocator, &queue.commands[i].pool);
                DEBUG_VK(res, "Failed to create command pool!");

                allocInfo.commandPool = queue.commands[i].pool;
                res = vkAllocateCommandBuffers(device, &allocInfo, &queue.commands[i].buffer);
                DEBUG_VK(res, "Failed to allocate command buffer!");

                queue.commands[i].staging = CreateBuffer(stagingBufferSize, Usage::TransferSrc, Memory::CPU, "StagingBuffer" + std::to_string(q) + "_" + std::to_string(i));
                vkMapMemory(device, queue.commands[i].staging.resource->memory, 0, stagingBufferSize, 0, (void**)&queue.commands[i].stagingCpu);
            }
        }
    }

    // 1 por frame in flight por queue family
    stagingBuffer = CreateBuffer(stagingBufferSize, Usage::TransferSrc, Memory::CPU, "Staging Buffer");
}

void Context::DestroyDevice() {
    stagingBuffer = {};
    for (int q = 0; q < Queue::Count; q++) {
        for (int i = 0; i < framesInFlight; i++) {
            vkDestroyCommandPool(device, queues[q].commands[i].pool, allocator);
            queues[q].commands[i].staging = {};
            queues[q].commands[i].stagingCpu = nullptr;
        }
    }
    vkDestroyCommandPool(device, commandPool, vkw::ctx().allocator);
    vkDestroyDevice(device, vkw::ctx().allocator);
    device = VK_NULL_HANDLE;
    commandPool = VK_NULL_HANDLE;
    graphicsQueue = VK_NULL_HANDLE;
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
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }

    // create image views
    swapChainViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        auto res = vkCreateImageView(device, &viewInfo, allocator, &swapChainViews[i]);
        DEBUG_VK(res, "Failed to create SwapChain image view!");
    }

    // create command buffers 
    {
        commandBuffers.resize(swapChainImages.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vkw::ctx().commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        auto res = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
        DEBUG_VK(res, "Failed to allocate command buffers!");
    }

    // synchronization objects
    {
        imageAvailableSemaphores.resize(framesInFlight);
        renderFinishedSemaphores.resize(framesInFlight);
        inFlightFences.resize(framesInFlight);
        imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
        for (size_t i = 0; i < framesInFlight; i++) {
            auto res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
            DEBUG_VK(res, "Failed to create semaphore!");
            res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
            DEBUG_VK(res, "Failed to create semaphore!");
            res = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
            DEBUG_VK(res, "Failed to create fence!");
        }
    }

    LOG_INFO("Create Swapchain");
    swapChainCurrentFrame = 0;
    swapChainDirty = false;

}

void Context::DestroySwapChain() {
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroyImageView(device, swapChainViews[i], allocator);
    }

    for (size_t i = 0; i < framesInFlight; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], allocator);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], allocator);
        vkDestroyFence(device, inFlightFences[i], allocator);
    }

    vkFreeCommandBuffers(device, vkw::ctx().commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
    vkDestroySwapchainKHR(device, swapChain, allocator);

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
    imagesInFlight.clear();
    commandBuffers.clear();
    swapChainViews.clear();
    swapChainImages.clear();
    swapChain = VK_NULL_HANDLE;
}

uint32_t Context::Acquire() {
    LUZ_PROFILE_FUNC();

    vkWaitForFences(device, 1, &inFlightFences[swapChainCurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    auto res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[swapChainCurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        swapChainDirty = true;
    }
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        DEBUG_VK(res, "Failed to acquire swap chain image!");
    }

    currentImageIndex = imageIndex;
    return imageIndex;
}

void Context::SubmitAndPresent(uint32_t imageIndex) {
    LUZ_PROFILE_FUNC();

    // check if a previous frame is using this image
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    // mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = inFlightFences[swapChainCurrentFrame];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[swapChainCurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(commandBuffers[imageIndex]);

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[swapChainCurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device, 1, &inFlightFences[swapChainCurrentFrame]);

    auto res = vkQueueSubmit(vkw::ctx().graphicsQueue, 1, &submitInfo, inFlightFences[swapChainCurrentFrame]);
    DEBUG_VK(res, "Failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    res = vkQueuePresentKHR(vkw::ctx().graphicsQueue, &presentInfo);

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

VkCommandBuffer Context::BeginSingleTimeCommands() {
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

void Context::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    auto res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    DEBUG_VK(res, "Failed to submit single time command buffer");
    res = vkQueueWaitIdle(graphicsQueue);
    DEBUG_VK(res, "Failed to wait idle command buffer");

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
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

}