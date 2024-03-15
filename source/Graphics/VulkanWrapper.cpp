#include "Luzpch.hpp"

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

#include "VulkanWrapper.h"

#include "LuzCommon.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// taken from Sam Lantiga: https://www.libsdl.org/tmp/SDL/test/testvulkan.c
static const char *VK_ERROR_STRING(VkResult result) {
    switch((int)result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
        return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    default:
        break;
    }
    if(result < 0)
        return "VK_ERROR_<Unknown>";
    return "VK_<Unknown>";
}

namespace vkw {

struct Context {
    void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset);
    void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset);
    void CmdCopy(Image& dst, void* data, uint32_t size);
    void CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset);
    void CmdBarrier(Image& img, Layout::ImageLayout layout);
    void CmdBarrier();
    void EndCommandBuffer(VkSubmitInfo submitInfo);

    void LoadShaders(Pipeline& pipeline);
    std::vector<char> CompileShader(const std::filesystem::path& path);
    void CreatePipeline(const PipelineDesc& desc, Pipeline& pipeline);

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    std::string applicationName = "Luz";
    std::string engineName = "Luz";
    VmaAllocator vmaAllocator;

    uint32_t apiVersion;
    std::vector<bool> activeLayers;
    std::vector<const char*> activeLayersNames;
    std::vector<VkLayerProperties> layers;
    std::vector<bool> activeExtensions;
    std::vector<const char*> activeExtensionsNames;
    std::vector<VkExtensionProperties> instanceExtensions;

    bool enableValidationLayers = true;

    std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
    };

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSampleCountFlagBits maxSamples = VK_SAMPLE_COUNT_1_BIT;
    VkSampleCountFlags sampleCounts;

    VkPhysicalDeviceFeatures physicalFeatures{};
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    VkPhysicalDeviceProperties physicalProperties{};

    std::vector<VkPresentModeKHR> availablePresentModes;
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;
    std::vector<VkExtensionProperties> availableExtensions;
    std::vector<VkQueueFamilyProperties> availableFamilies;

    // bindless resources
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet bindlessDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool bindlessDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout bindlessDescriptorLayout = VK_NULL_HANDLE;

    VkDevice device = VK_NULL_HANDLE;

    const uint32_t timeStampPerPool = 64;
    struct CommandResources {
        u8* stagingCpu = nullptr;
        uint32_t stagingOffset = 0;
        Buffer staging;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkQueryPool queryPool;
        std::vector<std::string> timeStampNames;
        std::vector<uint64_t> timeStamps;
    };
    struct InternalQueue {
        VkQueue queue = VK_NULL_HANDLE;
        int family = -1;
        std::vector<CommandResources> commands;
    };
    InternalQueue queues[Queue::Count];
    Queue currentQueue = Queue::Count;
    std::shared_ptr<PipelineResource> currentPipeline;
    const uint32_t stagingBufferSize = 256 * 1024 * 1024;

    VkPhysicalDeviceMemoryProperties memoryProperties;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<Image> swapChainImages;
    std::vector<VkImage> swapChainImageResources;
    std::vector<VkImageView> swapChainViews;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    uint32_t additionalImages = 0;
    uint32_t framesInFlight = 3;
    VkFormat depthFormat;
    VkExtent2D swapChainExtent;
    uint32_t swapChainCurrentFrame;
    bool swapChainDirty = true;
    uint32_t currentImageIndex = 0;

    std::vector<int32_t> availableBufferRID;
    std::vector<int32_t> availableImageRID;
    std::vector<int32_t> availableTLASRID;
    VkSampler genericSampler;

    // preferred, warn if not available
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    VkSampleCountFlagBits numSamples  = VK_SAMPLE_COUNT_1_BIT;

    const uint32_t initialScratchBufferSize = 64*1024*1024;
    Buffer asScratchBuffer;
    VkDeviceAddress asScratchAddress;

    std::map<std::string, float> timeStampTable;

    void CreateInstance(GLFWwindow* window);
    void DestroyInstance();

    void CreatePhysicalDevice();

    void CreateDevice();
    void DestroyDevice();

    void CreateImGui(GLFWwindow* window);

    void CreateSurfaceFormats();

    void CreateSwapChain(uint32_t width, uint32_t height);
    void DestroySwapChain();

    uint32_t FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties);
    bool SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);

    inline Image& GetCurrentSwapChainImage() {
        return swapChainImages[currentImageIndex];
    }
    inline CommandResources& GetCurrentCommandResources() {
        return queues[currentQueue].commands[currentImageIndex];
    }

    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkSampler CreateSampler(float maxLod);

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;

    vkw::Buffer dummyVertexBuffer;
};

static Context _ctx;

struct Resource {
    std::string name;
    int32_t rid = -1;
    virtual ~Resource() {};
};

struct BufferResource : Resource {
    VkBuffer buffer;
    VmaAllocation allocation;

    virtual ~BufferResource() {
        vmaDestroyBuffer(_ctx.vmaAllocator, buffer, allocation);
    }
};

struct ImageResource : Resource {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    bool fromSwapchain = false;
    std::vector<VkImageView> layersView;
    std::vector<ImTextureID> imguiRIDs;

    virtual ~ImageResource() {
        if (!fromSwapchain) {
            for (VkImageView layerView : layersView) {
                vkDestroyImageView(_ctx.device, layerView, _ctx.allocator);
            }
            layersView.clear();
            vkDestroyImageView(_ctx.device, view, _ctx.allocator);
            vmaDestroyImage(_ctx.vmaAllocator, image, allocation);
            if (rid >= 0) {
                _ctx.availableImageRID.push_back(rid);
                for (ImTextureID imguiRID : imguiRIDs) {
                    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)imguiRID);
                }
                rid = -1;
                imguiRIDs.clear();
            }
        }
    }
};

struct PipelineResource : Resource {
    VkPipeline pipeline;
    VkPipelineLayout layout;

    virtual ~PipelineResource() {
        vkDestroyPipeline(_ctx.device, pipeline, _ctx.allocator);
        vkDestroyPipelineLayout(_ctx.device, layout, _ctx.allocator);
    }
};

struct TLASResource : Resource {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    Buffer buffer;
    Buffer instancesBuffer;
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
    VkAccelerationStructureGeometryKHR topASGeometry = {};

    virtual ~TLASResource() {
        _ctx.vkDestroyAccelerationStructureKHR(_ctx.device, accel, _ctx.allocator);
        buffer = {};
        instancesBuffer = {};
    }
};

struct BLASResource : Resource {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    vkw::Buffer buffer;
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
    VkAccelerationStructureGeometryKHR asGeom = {};
    VkAccelerationStructureBuildRangeInfoKHR offset;

    virtual ~BLASResource() {
        _ctx.vkDestroyAccelerationStructureKHR(_ctx.device, accel, _ctx.allocator);
        buffer = {};
    }
};

uint32_t Buffer::RID() {
    DEBUG_ASSERT(resource->rid != -1, "Invalid buffer rid");
    return uint32_t(resource->rid);
}

uint32_t TLAS::RID() {
    DEBUG_ASSERT(resource->rid != -1, "Invalid tlas rid");
    return uint32_t(resource->rid);
}

uint32_t Image::RID() {
    DEBUG_ASSERT(resource->rid != -1, "Invalid image rid");
    return uint32_t(resource->rid);
}

ImTextureID Image::ImGuiRID() {
    if (!resource || resource->rid == -1 || resource->imguiRIDs.size() == 0) {
        return nullptr;
    }
    return resource->imguiRIDs[0];
}

ImTextureID Image::ImGuiRID(uint32_t layer) {
    if (!resource || resource->rid == -1 || resource->imguiRIDs.size() <= layer) {
        return nullptr;
    }
    return resource->imguiRIDs[layer];
}

void Init(GLFWwindow* window, uint32_t width, uint32_t height) {
    _ctx.CreateInstance(window);
    _ctx.CreatePhysicalDevice();
    _ctx.CreateDevice();
    _ctx.CreateSurfaceFormats();
    _ctx.CreateSwapChain(width, height);
    _ctx.CreateImGui(window);
}

void ImGuiCheckVulkanResult(VkResult res) {
    if (res == 0) {
        return;
    }
    std::cerr << "vulkan error during some imgui operation: " << res << '\n';
    if (res < 0) {
        throw std::runtime_error("");
    }
}

void Context::CreateImGui(GLFWwindow* window) {
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = _ctx.instance;
    initInfo.PhysicalDevice = _ctx.physicalDevice;
    initInfo.Device = _ctx.device;
    initInfo.QueueFamily = _ctx.queues[vkw::Queue::Graphics].family;
    initInfo.Queue = _ctx.queues[vkw::Queue::Graphics].queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = _ctx.imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = (uint32_t)_ctx.swapChainImages.size();
    initInfo.MSAASamples = _ctx.numSamples;
    initInfo.Allocator = _ctx.allocator;
    initInfo.CheckVkResultFn = ImGuiCheckVulkanResult;
    initInfo.UseDynamicRendering = true;
    initInfo.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
    ImGui_ImplVulkan_Init(&initInfo, nullptr);
    ImGui_ImplVulkan_CreateFontsTexture();
    ImGui_ImplGlfw_InitForVulkan(window, true);
}

void OnSurfaceUpdate(uint32_t width, uint32_t height) {
    _ctx.DestroySwapChain();
    _ctx.CreateSurfaceFormats();
    _ctx.CreateSwapChain(width, height);
}

void Destroy() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
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

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if (memory & Memory::CPU) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    auto result = vmaCreateBuffer(_ctx.vmaAllocator, &bufferInfo, &allocInfo, &res->buffer, &res->allocation, nullptr);
    DEBUG_VK(result, "Failed to create buffer!");

    Buffer buffer = {
        .resource = res,
        .size = size,
        .usage = usage,
        .memory = memory,
    };

    if (usage & BufferUsage::Storage) {
        res->rid = _ctx.availableBufferRID.back();
        _ctx.availableBufferRID.pop_back();
        VkDescriptorBufferInfo descriptorInfo = {};
        VkWriteDescriptorSet write = {};
        descriptorInfo.buffer = res->buffer;
        descriptorInfo.offset = 0;
        descriptorInfo.range = size;
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _ctx.bindlessDescriptorSet;
        write.dstBinding = LUZ_BINDING_BUFFER;
        write.dstArrayElement = buffer.RID();
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
    imageInfo.arrayLayers = desc.layers;
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
    if (desc.layers == 6) {
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    auto result = vmaCreateImage(_ctx.vmaAllocator, &imageInfo, &allocInfo, &res->image, &res->allocation, nullptr);
    DEBUG_VK(result, "Failed to create image!");

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
    if (desc.layers == 1) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    } else {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    }
    viewInfo.format = (VkFormat)desc.format;
    viewInfo.subresourceRange.aspectMask = (VkImageAspectFlags)aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.layers;

    result = vkCreateImageView(device, &viewInfo, allocator, &res->view);
    DEBUG_VK(result, "Failed to create image view!");

    if (desc.layers > 1) {
        viewInfo.subresourceRange.layerCount = 1;
        res->layersView.resize(desc.layers);
        for (int i = 0; i < desc.layers; i++) {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.subresourceRange.baseArrayLayer = i;
            result = vkCreateImageView(device, &viewInfo, allocator, &res->layersView[i]);
            DEBUG_VK(result, "Failed to create image view!");
        }
    }

    Image image = {
        .resource = res,
        .width = desc.width,
        .height = desc.height,
        .usage = desc.usage,
        .format = desc.format,
        .layout = Layout::Undefined,
        .aspect = aspect,
        .layers = desc.layers,
    };

    if (desc.usage & ImageUsage::Sampled) {
        Layout::ImageLayout newLayout = Layout::ShaderRead;
        if (aspect == (Aspect::Depth | Aspect::Stencil)) {
            newLayout = Layout::DepthStencilRead;
        } else if (aspect == Aspect::Depth) {
            newLayout = Layout::DepthRead;
        }
        res->imguiRIDs.resize(desc.layers);
        if (desc.layers > 1) {
            for (int i = 0; i < desc.layers; i++) {
                res->imguiRIDs[i] = ImGui_ImplVulkan_AddTexture(_ctx.genericSampler, res->layersView[i], (VkImageLayout)newLayout);
            }
        } else {
            res->imguiRIDs[0] = ImGui_ImplVulkan_AddTexture(_ctx.genericSampler, res->view, (VkImageLayout)newLayout);
        }
        res->rid = _ctx.availableImageRID.back();
        _ctx.availableImageRID.pop_back();

        VkDescriptorImageInfo descriptorInfo = {
            .sampler = _ctx.genericSampler,
            .imageView = res->view,
            .imageLayout = (VkImageLayout)newLayout,
        };
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _ctx.bindlessDescriptorSet;
        write.dstBinding = LUZ_BINDING_TEXTURE;
        write.dstArrayElement = image.RID();
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

Pipeline CreatePipeline(const PipelineDesc& desc) {
    Pipeline pipeline;
    pipeline.resource = std::make_shared<PipelineResource>();
    pipeline.stages = desc.stages;
    _ctx.LoadShaders(pipeline);
    _ctx.CreatePipeline(desc, pipeline);
    return pipeline;
}

TLAS CreateTLAS(uint32_t maxInstances, const std::string& name) {
    TLAS tlas;
    tlas.resource = std::make_shared<TLASResource>();
    tlas.resource->name = name;
    std::shared_ptr<TLASResource>& tlasRes = tlas.resource;
    tlasRes->rid = _ctx.availableTLASRID.back();
    _ctx.availableTLASRID.pop_back();
    auto& device = _ctx.device;

    tlasRes->instancesBuffer = vkw::CreateBuffer(sizeof(VkAccelerationStructureInstanceKHR) * maxInstances, vkw::BufferUsage::AccelerationStructureInput, vkw::Memory::GPU);
    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = tlasRes->instancesBuffer.resource->buffer;
    VkDeviceAddress instancesBufferAddr = vkGetBufferDeviceAddress(device, &bufferInfo);

    // Wraps a device pointer to the above uploaded instances.
    VkAccelerationStructureGeometryInstancesDataKHR instancesVk{};
    instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesVk.data.deviceAddress = instancesBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    tlasRes->topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasRes->topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasRes->topASGeometry.geometry.instances = instancesVk;

    tlasRes->buildInfo = {};
    tlasRes->buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasRes->buildInfo.flags = flags;
    tlasRes->buildInfo.geometryCount = 1;
    tlasRes->buildInfo.pGeometries = &tlasRes->topASGeometry;
    tlasRes->buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasRes->buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasRes->buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    _ctx.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasRes->buildInfo, &maxInstances, &sizeInfo);

    tlasRes->buffer = vkw::CreateBuffer(sizeInfo.accelerationStructureSize, vkw::BufferUsage::AccelerationStructure, vkw::Memory::GPU);
    if (_ctx.asScratchBuffer.size < sizeInfo.buildScratchSize) {
        _ctx.asScratchBuffer = vkw::CreateBuffer(sizeInfo.buildScratchSize, vkw::BufferUsage::Address | vkw::BufferUsage::Storage, vkw::Memory::GPU);
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.buffer = tlasRes->buffer.resource->buffer;
    _ctx.vkCreateAccelerationStructureKHR(device, &createInfo, _ctx.allocator, &tlasRes->accel);

    VkDebugUtilsObjectNameInfoEXT vkName = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = VkObjectType::VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
    .objectHandle = (uint64_t)tlasRes->accel,
    .pObjectName = name.c_str(),
    };
    _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &vkName);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructure{};
    descriptorAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructure.accelerationStructureCount = 1;
    descriptorAccelerationStructure.pAccelerationStructures = &tlasRes->accel;

    std::vector<VkWriteDescriptorSet> writes;

    VkWriteDescriptorSet writeBindlessAccelerationStructure{};
    writeBindlessAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeBindlessAccelerationStructure.pNext = &descriptorAccelerationStructure;
    writeBindlessAccelerationStructure.dstSet = _ctx.bindlessDescriptorSet;
    writeBindlessAccelerationStructure.dstBinding = LUZ_BINDING_TLAS;
    writeBindlessAccelerationStructure.dstArrayElement = tlas.RID();
    writeBindlessAccelerationStructure.descriptorCount = 1;
    writeBindlessAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    writes.push_back(writeBindlessAccelerationStructure);

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
    DEBUG_TRACE("Update descriptor sets in CreateTLAS!");
    // Update build information
    tlasRes->buildInfo.srcAccelerationStructure = tlasRes->accel;
    tlasRes->buildInfo.dstAccelerationStructure = tlasRes->accel;
    tlasRes->buildInfo.scratchData.deviceAddress = _ctx.asScratchAddress;

    return tlas;
}

BLAS CreateBLAS(const BLASDesc& desc) {
    auto& device = _ctx.device;
    BLAS blas;
    blas.resource = std::make_shared<BLASResource>();
    blas.resource->name = desc.name;
    std::shared_ptr<BLASResource> res = blas.resource;

    // BLAS builder requires raw device addresses.
    VkBufferDeviceAddressInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vertexBufferInfo.buffer = desc.vertexBuffer.resource->buffer;
    vertexBufferInfo.pNext = nullptr;
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device, &vertexBufferInfo);
    VkBufferDeviceAddressInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    indexBufferInfo.buffer = desc.indexBuffer.resource->buffer;
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device, &indexBufferInfo);

    u32 maxPrimitiveCount = desc.indexCount / 3;

    // Describe buffer as array of VertexObj.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = desc.vertexStride;
    triangles.maxVertex = desc.vertexCount;
    // Describe index data (32-bit unsigned int)
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    // triangles.transformData = {};

    // Identify the above data as containing opaque triangles.
    res->asGeom = {};
    res->asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    res->asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    res->asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    res->asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    res->offset.firstVertex = 0;
    res->offset.primitiveCount = maxPrimitiveCount;
    res->offset.primitiveOffset = 0;
    res->offset.transformOffset = 0;

    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    // Preparing the information for the acceleration build commands.
    // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
    // Other information will be filled in the createBlas (see #2)
    res->buildInfo = {};
    res->buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    res->buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    res->buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    // Our blas is made from only one geometry, but could be made of many geometries
    res->buildInfo.flags = flags;
    res->buildInfo.geometryCount = 1;
    res->buildInfo.pGeometries = &res->asGeom;

    // Build range information
    res->rangeInfo = &res->offset;

    // Finding sizes to create acceleration structures and scratch
    uint32_t primCount = res->offset.primitiveCount;
    res->sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    _ctx.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &res->buildInfo, &primCount, &res->sizeInfo);

    // Actual allocation of buffer and acceleration structure.
    res->buffer = vkw::CreateBuffer(res->sizeInfo.accelerationStructureSize, vkw::BufferUsage::AccelerationStructure, vkw::Memory::GPU);

    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    createInfo.size = res->sizeInfo.accelerationStructureSize; // Will be used to allocate memory.
    createInfo.buffer = res->buffer.resource->buffer;
    _ctx.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &res->accel);

    VkDebugUtilsObjectNameInfoEXT vkName = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = VkObjectType::VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
    .objectHandle = (uint64_t)res->accel,
    .pObjectName = desc.name.c_str(),
    };
    _ctx.vkSetDebugUtilsObjectNameEXT(_ctx.device, &vkName);

    if (_ctx.asScratchBuffer.size < res->sizeInfo.buildScratchSize) {
        _ctx.asScratchBuffer = vkw::CreateBuffer(res->sizeInfo.buildScratchSize, vkw::BufferUsage::Storage, vkw::Memory::GPU);
    }
    
    res->buildInfo.dstAccelerationStructure = res->accel; // Setting where the build lands
    res->buildInfo.scratchData.deviceAddress = _ctx.asScratchAddress; // All build are using the same scratch buffer

    return blas;
}

void Context::LoadShaders(Pipeline& pipeline) {
    pipeline.stageBytes.clear();
    for (auto& stage : pipeline.stages) {
        pipeline.stageBytes.push_back(CompileShader(stage.path));
    }
}

std::vector<char> Context::CompileShader(const std::filesystem::path& path) {
    char compile_string[1024];
    char inpath[256];
    char outpath[256];
    std::string cwd = std::filesystem::current_path().string();
    sprintf(inpath, "%s/source/Shaders/%s", cwd.c_str(), path.string().c_str());
    sprintf(outpath, "%s/bin/%s.spv", cwd.c_str(), path.filename().string().c_str());
    sprintf(compile_string, "%s -V %s -o %s --target-env spirv1.4", GLSL_VALIDATOR, inpath, outpath);
    DEBUG_TRACE("[ShaderCompiler] Command: {}", compile_string);
    DEBUG_TRACE("[ShaderCompiler] Output:");
    if(system(compile_string)) {
        LOG_ERROR("[ShaderCompiler] Error!");
    }

    // 'ate' specify to start reading at the end of the file
    // then we can use the read position to determine the size of the file
    std::ifstream file(outpath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_CRITICAL("Failed to open file: '{}'", outpath);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void Context::CreatePipeline(const PipelineDesc& desc, Pipeline& pipeline) {
    pipeline.point = desc.point;
    pipeline.resource->name = desc.name;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // fragments beyond near and far planes are clamped to them
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // line thickness in terms of number of fragments
    rasterizer.lineWidth = 1.0f;
    if (desc.cullFront) {
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    } else {
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    }
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 0.5f;
    multisampling.pSampleMask = nullptr;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(desc.stages.size());
    std::vector<VkShaderModule> shaderModules(desc.stages.size());
    for (int i = 0; i < desc.stages.size(); i++) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = pipeline.stageBytes[i].size();
        createInfo.pCode = (const uint32_t*)(pipeline.stageBytes[i].data());
        auto result = vkCreateShaderModule(device, &createInfo, allocator, &shaderModules[i]);
        DEBUG_VK(result, "Failed to create shader module!");
        shaderStages[i] = {};
        shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[i].stage = (VkShaderStageFlagBits)desc.stages[i].stage;
        shaderStages[i].module = shaderModules[i];
        // function to invoke inside the shader module
        // it's possible to combine multiple shaders into a single module
        // using different entry points
        shaderStages[i].pName = "main";
        // this allow us to specify values for shader constants
        shaderStages[i].pSpecializationInfo = nullptr;
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescs(desc.vertexAttributes.size());
    uint32_t attributeSize = 0;
    for (int i = 0; i < desc.vertexAttributes.size(); i++) {
        attributeDescs[i].binding = 0;
        attributeDescs[i].location = i;
        attributeDescs[i].format = (VkFormat)desc.vertexAttributes[i];
        attributeDescs[i].offset = attributeSize;
        if (desc.vertexAttributes[i] == Format::RG32_sfloat) {
            attributeSize += 2 * sizeof(float);
        } else if (desc.vertexAttributes[i] == Format::RGB32_sfloat) {
            attributeSize += 3 * sizeof(float);
        } else if (desc.vertexAttributes[i] == Format::RGBA32_sfloat) {
            attributeSize += 4 * sizeof(float);
        } else {
            DEBUG_ASSERT(false, "Invalid Vertex Attribute");
        }
    }

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = attributeSize;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)(attributeDescs.size());
    // these points to an array of structs that describe how to load the vertex data
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

    // define the type of input of our pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // with this parameter true we can break up lines and triangles in _STRIP topology modes
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    std::vector<VkDynamicState> dynamicStates;
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamicCreate = {};
    dynamicCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicCreate.pDynamicStates = dynamicStates.data();
    dynamicCreate.dynamicStateCount = dynamicStates.size();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    std::vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(bindlessDescriptorLayout);

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = 256;
    pushConstant.stageFlags = VK_SHADER_STAGE_ALL;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    auto vkRes = vkCreatePipelineLayout(device, &pipelineLayoutInfo, allocator, &pipeline.resource->layout);
    DEBUG_VK(vkRes, "Failed to create pipeline layout!");

    VkPipelineRenderingCreateInfoKHR pipelineRendering{};
    pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRendering.colorAttachmentCount = desc.colorFormats.size();
    pipelineRendering.pColorAttachmentFormats = (VkFormat*)desc.colorFormats.data();
    pipelineRendering.depthAttachmentFormat = desc.useDepth ? (VkFormat)desc.depthFormat : VK_FORMAT_UNDEFINED;
    pipelineRendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRendering.viewMask = 0;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(desc.colorFormats.size());
    for (int i = 0; i < desc.colorFormats.size(); i++) {
        blendAttachments[i].colorWriteMask =  VK_COLOR_COMPONENT_R_BIT;
        blendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        blendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        blendAttachments[i].blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.attachmentCount = blendAttachments.size();
    colorBlendState.pAttachments = blendAttachments.data();
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = &dynamicCreate;
    pipelineInfo.layout = pipeline.resource->layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    // pipelineInfo.renderPass = SwapChain::GetRenderPass();
    pipelineInfo.subpass = 0;
    // if we were creating this pipeline by deriving it from another
    // we should specify here
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.pNext = &pipelineRendering;

    vkRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, allocator, &pipeline.resource->pipeline);
    DEBUG_VK(vkRes, "Failed to create graphics pipeline!");

    for (int i = 0; i < shaderModules.size(); i++) {
        vkDestroyShaderModule(device, shaderModules[i], allocator);
    }
}

void CmdBuildBLAS(BLAS& blas) {
    auto& device = _ctx.device;
    auto& allocator = _ctx.device;
    auto& cmd = _ctx.GetCurrentCommandResources();
    std::shared_ptr<BLASResource>& res = blas.resource;

    // Building the bottom-level-acceleration-structure
    _ctx.vkCmdBuildAccelerationStructuresKHR(cmd.buffer, 1, &res->buildInfo, &res->rangeInfo);

    // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
    // is finished before starting the next one.
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd.buffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void CmdBuildTLAS(TLAS& tlas, const std::vector<BLASInstance>& blasInstances) {
    auto& device = _ctx.device;
    auto& cmd = _ctx.GetCurrentCommandResources();
    std::shared_ptr<TLASResource>& res = tlas.resource;

    std::vector<VkAccelerationStructureInstanceKHR> instances(blasInstances.size());
    for (int i = 0; i < instances.size(); i++) {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = blasInstances[i].blas.resource->accel;
        instances[i] = {};
        instances[i].instanceCustomIndex = blasInstances[i].customIndex;
        instances[i].accelerationStructureReference = _ctx.vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
        instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instances[i].mask = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        for (int x = 0; x < 3; x++) {
            for (int y = 0; y < 4; y++) {
                instances[i].transform.matrix[x][y] = blasInstances[i].modelMat[y][x];
            }
        }
    }

    CmdCopy(res->instancesBuffer, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(cmd.buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo = {instances.size(), 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;
    _ctx.vkCmdBuildAccelerationStructuresKHR(cmd.buffer, 1, &res->buildInfo, &pBuildOffsetInfo);
    // todo: hash and only update mode if nothing changed
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

void CmdBeginRendering(const std::vector<Image>& colorAttachs, Image depthAttach, uint32_t layerCount) {
    auto& cmd = _ctx.GetCurrentCommandResources();

    glm::ivec2 offset(0, 0);
    glm::ivec2 extent(0, 0);
    if (colorAttachs.size() > 0) {
        extent.x = colorAttachs[0].width;
        extent.y = colorAttachs[0].height;
    } else if (depthAttach.resource) {
        extent.x = depthAttach.width;
        extent.y = depthAttach.height;
    }

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.viewMask = 0;
    renderingInfo.layerCount = layerCount;
    renderingInfo.renderArea.extent = { uint32_t(extent.x), uint32_t(extent.y) };
    renderingInfo.renderArea.offset = { offset.x, offset.y };
    renderingInfo.flags = 0;

    std::vector<VkRenderingAttachmentInfoKHR> colorAttachInfos(colorAttachs.size());
    VkRenderingAttachmentInfoKHR depthAttachInfo;
    for (int i = 0; i < colorAttachs.size(); i++) {
        colorAttachInfos[i] = {};
        colorAttachInfos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAttachInfos[i].imageView = colorAttachs[i].resource->view;
        colorAttachInfos[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachInfos[i].resolveMode = VK_RESOLVE_MODE_NONE;
        colorAttachInfos[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachInfos[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachInfos[i].clearValue.color = { 0, 0, 0, 1 };
    }

    renderingInfo.colorAttachmentCount = colorAttachInfos.size();
    renderingInfo.pColorAttachments = colorAttachInfos.data();
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    if (depthAttach.resource) {
        depthAttachInfo = {};
        depthAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        depthAttachInfo.imageView = depthAttach.resource->view;
        depthAttachInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachInfo.clearValue.depthStencil = { 1.0f, 0 };
        renderingInfo.pDepthAttachment = &depthAttachInfo;
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.x);
    viewport.height = static_cast<float>(extent.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.offset = { offset.x, offset.y };
    scissor.extent.width = extent.x;
    scissor.extent.height = extent.y;
    vkCmdSetViewport(cmd.buffer, 0, 1, &viewport);
    vkCmdSetScissor(cmd.buffer, 0, 1, &scissor);

    vkCmdBeginRendering(cmd.buffer, &renderingInfo);
}

void CmdEndRendering() {
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkCmdEndRendering(cmd.buffer);
}

void CmdBeginPresent() {
    vkw::CmdBarrier(_ctx.GetCurrentSwapChainImage(), vkw::Layout::ColorAttachment);
    vkw::CmdBeginRendering({ _ctx.GetCurrentSwapChainImage() }, {});
}

void CmdEndPresent() {
    vkw::CmdEndRendering();
    vkw::CmdBarrier(_ctx.GetCurrentSwapChainImage(), vkw::Layout::Present);
}

void CmdDrawMesh(Buffer& vertexBuffer, Buffer& indexBuffer, uint32_t indexCount) {
    auto& cmd = _ctx.GetCurrentCommandResources();
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd.buffer, 0, 1, &vertexBuffer.resource->buffer, offsets);
    vkCmdBindIndexBuffer(cmd.buffer, indexBuffer.resource->buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd.buffer, indexCount, 1, 0, 0, 0);
}

void CmdDrawPassThrough() {
    auto& cmd = _ctx.GetCurrentCommandResources();
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd.buffer, 0, 1, &_ctx.dummyVertexBuffer.resource->buffer, offsets);
    vkCmdDraw(cmd.buffer, 6, 1, 0, 0);
}

void CmdDrawImGui(ImDrawData* data) {
    ImGui_ImplVulkan_RenderDrawData(data, _ctx.GetCurrentCommandResources().buffer);
}

int CmdBeginTimeStamp(const std::string& name) {
    DEBUG_ASSERT(_ctx.currentQueue != Queue::Transfer, "Time Stamp not supported in Transfer queue");
    auto& cmd = _ctx.GetCurrentCommandResources();
    int id = cmd.timeStamps.size();
    if (id >= _ctx.timeStampPerPool - 1) {
        LOG_WARN("Maximum number of time stamp per pool exceeded. Ignoring Time stamp {}", name.c_str());
        return -1;
    }
    vkCmdWriteTimestamp(cmd.buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, cmd.queryPool, id);
    cmd.timeStamps.push_back(0);
    cmd.timeStamps.push_back(0);
    cmd.timeStampNames.push_back(name);
    return id;
}

void CmdEndTimeStamp(int timeStampIndex) {
    if (timeStampIndex >= 0 && timeStampIndex < _ctx.timeStampPerPool - 1) {
        auto& cmd = _ctx.GetCurrentCommandResources();
        vkCmdWriteTimestamp(cmd.buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, cmd.queryPool, timeStampIndex + 1);
    }
}

void CmdBindPipeline(Pipeline& pipeline) {
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkCmdBindPipeline(cmd.buffer, (VkPipelineBindPoint)pipeline.point, pipeline.resource->pipeline);
    vkCmdBindDescriptorSets(cmd.buffer, (VkPipelineBindPoint)pipeline.point, pipeline.resource->layout, 0, 1, &_ctx.bindlessDescriptorSet, 0, nullptr);

    _ctx.currentPipeline = pipeline.resource;
}

void CmdPushConstants(void* data, uint32_t size) {
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkCmdPushConstants(cmd.buffer, _ctx.currentPipeline->layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void BeginImGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void BeginCommandBuffer(Queue queue) {
    ASSERT(_ctx.currentQueue == Queue::Count, "Already recording a command buffer");
    _ctx.currentQueue = queue;
    auto& cmd = _ctx.GetCurrentCommandResources();
    vkWaitForFences(_ctx.device, 1, &cmd.fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_ctx.device, 1, &cmd.fence);

    if (cmd.timeStamps.size() > 0) {
        vkGetQueryPoolResults(_ctx.device, cmd.queryPool, 0, cmd.timeStamps.size(), cmd.timeStamps.size() * sizeof(uint64_t), cmd.timeStamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        for (int i = 0; i < cmd.timeStampNames.size(); i++) {
            const uint64_t begin = cmd.timeStamps[2 * i];
            const uint64_t end = cmd.timeStamps[2 * i + 1];
            _ctx.timeStampTable[cmd.timeStampNames[i]] = float(end - begin) * _ctx.physicalProperties.limits.timestampPeriod / 1000000.0f;
        }
        cmd.timeStamps.clear();
        cmd.timeStampNames.clear();
    }

    Context::InternalQueue& iqueue = _ctx.queues[queue];
    vkResetCommandPool(_ctx.device, cmd.pool, 0);
    cmd.stagingOffset = 0;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd.buffer, &beginInfo);

    if (queue != Queue::Transfer) {
        vkCmdResetQueryPool(cmd.buffer, cmd.queryPool, 0, _ctx.timeStampPerPool);
    }
}

void Context::EndCommandBuffer(VkSubmitInfo submitInfo) {
    auto& cmd = GetCurrentCommandResources();

    vkEndCommandBuffer(cmd.buffer);

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd.buffer;

    auto res = vkQueueSubmit(queues[currentQueue].queue, 1, &submitInfo, cmd.fence);
    DEBUG_VK(res, "Failed to submit command buffer");
}

void EndCommandBuffer() {
    _ctx.EndCommandBuffer({});
    _ctx.currentQueue = vkw::Queue::Count;
    _ctx.currentPipeline = {};
}

void WaitQueue(Queue queue) {
    // todo: wait on fence
    auto res = vkQueueWaitIdle(_ctx.queues[queue].queue);
    DEBUG_VK(res, "Failed to wait idle command buffer");
}

void WaitIdle() {
    auto res = vkDeviceWaitIdle(_ctx.device);
    DEBUG_VK(res, "Failed to wait idle command buffer");
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
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.geometryShader = VK_TRUE;
    if (supportedFeatures.logicOp)           { features2.features.logicOp           = VK_TRUE; }
    if (supportedFeatures.samplerAnisotropy) { features2.features.samplerAnisotropy = VK_TRUE; }
    if (supportedFeatures.sampleRateShading) { features2.features.sampleRateShading = VK_TRUE; }
    if (supportedFeatures.fillModeNonSolid)  { features2.features.fillModeNonSolid  = VK_TRUE; }
    if (supportedFeatures.wideLines)         { features2.features.wideLines         = VK_TRUE; }
    if (supportedFeatures.depthClamp)        { features2.features.depthClamp        = VK_TRUE; }

    auto requiredExtensions = _ctx.requiredExtensions;
    auto allExtensions = _ctx.availableExtensions;
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
    bufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
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

    features2.pNext = &sync2Features;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.pEnabledFeatures;
    createInfo.pNext = &features2;

    // specify the required layers to the device 
    if (_ctx.enableValidationLayers) {
        auto& layers = activeLayersNames;
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    auto res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    DEBUG_VK(res, "Failed to create logical device!");

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator);

    for (int q = 0; q < Queue::Count; q++) {
        vkGetDeviceQueue(device, queues[q].family, 0, &queues[q].queue);
    }

    genericSampler = CreateSampler(1.0);
    vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
    vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");

    VkDescriptorPoolSize imguiPoolSizes[]    = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} };
    VkDescriptorPoolCreateInfo imguiPoolInfo{};
    imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imguiPoolInfo.maxSets = (uint32_t)(1024);
    imguiPoolInfo.poolSizeCount = sizeof(imguiPoolSizes)/sizeof(VkDescriptorPoolSize);
    imguiPoolInfo.pPoolSizes = imguiPoolSizes;

    VkResult result = vkCreateDescriptorPool(device, &imguiPoolInfo, allocator, &imguiDescriptorPool);
    DEBUG_VK(result, "Failed to create imgui descriptor pool!");

    // create bindless resources
    {
        const u32 MAX_STORAGE = 4096;
        const u32 MAX_SAMPLEDIMAGES = 4096;
        const u32 MAX_ACCELERATIONSTRUCTURE = 64;

        for (int i = 0; i < MAX_STORAGE; i++) {
            availableBufferRID.push_back(i);
        }
        for (int i = 0; i < MAX_SAMPLEDIMAGES; i++) {
            availableImageRID.push_back(i);
        }
        for (int i = 0; i < MAX_ACCELERATIONSTRUCTURE; i++) {
            availableTLASRID.push_back(i);
        }

        // create descriptor set pool for bindless resources
        std::vector<VkDescriptorPoolSize> bindlessPoolSizes = { 
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SAMPLEDIMAGES},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_ACCELERATIONSTRUCTURE}
        };

        VkDescriptorPoolCreateInfo bindlessPoolInfo{};
        bindlessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        bindlessPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        bindlessPoolInfo.maxSets = 1;
        bindlessPoolInfo.poolSizeCount = bindlessPoolSizes.size();
        bindlessPoolInfo.pPoolSizes = bindlessPoolSizes.data();

        result = vkCreateDescriptorPool(device, &bindlessPoolInfo, allocator, &bindlessDescriptorPool);
        DEBUG_VK(result, "Failed to create bindless descriptor pool!");

        // create descriptor set layout for bindless resources
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> bindingFlags;

        VkDescriptorSetLayoutBinding texturesBinding{};
        texturesBinding.binding = LUZ_BINDING_TEXTURE;
        texturesBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texturesBinding.descriptorCount = MAX_SAMPLEDIMAGES;
        texturesBinding.stageFlags = VK_SHADER_STAGE_ALL;
        bindings.push_back(texturesBinding);
        bindingFlags.push_back({ VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT });

        VkDescriptorSetLayoutBinding storageBuffersBinding{};
        storageBuffersBinding.binding = LUZ_BINDING_BUFFER;
        storageBuffersBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storageBuffersBinding.descriptorCount = MAX_STORAGE;
        storageBuffersBinding.stageFlags = VK_SHADER_STAGE_ALL;
        bindings.push_back(storageBuffersBinding);
        bindingFlags.push_back({ VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT });

        VkDescriptorSetLayoutBinding accelerationStructureBinding{};
        accelerationStructureBinding.binding = LUZ_BINDING_TLAS;
        accelerationStructureBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        accelerationStructureBinding.descriptorCount = MAX_ACCELERATIONSTRUCTURE;
        accelerationStructureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(accelerationStructureBinding);
        bindingFlags.push_back({ VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT });

        VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
        setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        setLayoutBindingFlags.bindingCount = bindingFlags.size();
        setLayoutBindingFlags.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = bindings.size();
        descriptorLayoutInfo.pBindings = bindings.data();
        descriptorLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descriptorLayoutInfo.pNext = &setLayoutBindingFlags;

        result = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, allocator, &bindlessDescriptorLayout);
        DEBUG_VK(result, "Failed to create bindless descriptor set layout!");

        // create descriptor set for bindless resources
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = bindlessDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &bindlessDescriptorLayout;

        result = vkAllocateDescriptorSets(device, &allocInfo, &bindlessDescriptorSet);
        DEBUG_VK(result, "Failed to allocate bindless descriptor set!");
    }

    asScratchBuffer = vkw::CreateBuffer(initialScratchBufferSize, vkw::BufferUsage::Address | vkw::BufferUsage::Storage, vkw::Memory::GPU);
    VkBufferDeviceAddressInfo scratchInfo{};
    scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchInfo.buffer = asScratchBuffer.resource->buffer;
    asScratchAddress = vkGetBufferDeviceAddress(device, &scratchInfo);

    dummyVertexBuffer = vkw::CreateBuffer(
        6 * 3 * sizeof(float),
        vkw::BufferUsage::Vertex | vkw::BufferUsage::AccelerationStructureInput,
        vkw::Memory::GPU,
        "VertexBuffer#Dummy"
    );
}

void Context::DestroyDevice() {
    dummyVertexBuffer = {};
    currentPipeline = {};
    asScratchBuffer = {};
    vkDestroyDescriptorPool(device, imguiDescriptorPool, allocator);
    vkDestroyDescriptorPool(device, bindlessDescriptorPool, allocator);
    vkDestroyDescriptorSetLayout(device, bindlessDescriptorLayout, allocator);
    bindlessDescriptorSet = VK_NULL_HANDLE;
    bindlessDescriptorPool = VK_NULL_HANDLE;
    bindlessDescriptorLayout = VK_NULL_HANDLE;
    vmaDestroyAllocator(vmaAllocator);
    vkDestroySampler(device, genericSampler, allocator);
    vkDestroyDevice(device, allocator);
    device = VK_NULL_HANDLE;
}

void Context::CreateSurfaceFormats() {
    auto surface = _ctx.surface;

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
        createInfo.surface = _ctx.surface;
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
                queue.commands[i].stagingCpu = (u8*)queue.commands[i].staging.resource->allocation->GetMappedData();

                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                vkCreateFence(device, &fenceInfo, allocator, &queue.commands[i].fence);

                VkQueryPoolCreateInfo queryPoolInfo{};
                queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
                queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
                queryPoolInfo.queryCount = timeStampPerPool;
                res = vkCreateQueryPool(device, &queryPoolInfo, allocator, &queue.commands[i].queryPool);
                DEBUG_VK(res, "failed to create query pool");

                queue.commands[i].timeStamps.clear();
                queue.commands[i].timeStampNames.clear();
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
            vkDestroyQueryPool(device, queues[q].commands[i].queryPool, allocator);
        }
    }

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    swapChainViews.clear();
    swapChainImages.clear();
    swapChain = VK_NULL_HANDLE;
}

void AcquireImage() {
    LUZ_PROFILE_FUNC();

    uint32_t imageIndex;
    auto res = vkAcquireNextImageKHR(_ctx.device, _ctx.swapChain, UINT64_MAX, _ctx.imageAvailableSemaphores[_ctx.swapChainCurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        _ctx.swapChainDirty = true;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        DEBUG_VK(res, "Failed to acquire swap chain image!");
    }

    _ctx.currentImageIndex = imageIndex;
}

bool GetSwapChainDirty() {
    return _ctx.swapChainDirty;
}

void GetTimeStamps(std::map<std::string, float>& timeTable) {
    timeTable = _ctx.timeStampTable;
}

void SubmitAndPresent() {
    LUZ_PROFILE_FUNC();

    auto& cmd = _ctx.GetCurrentCommandResources();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { _ctx.imageAvailableSemaphores[_ctx.swapChainCurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(cmd.buffer);

    VkSemaphore signalSemaphores[] = { _ctx.renderFinishedSemaphores[_ctx.swapChainCurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { _ctx.swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &_ctx.currentImageIndex;
    presentInfo.pResults = nullptr;

    //vkEndCommandBuffer(cmd.buffer);
    //auto res = vkQueueSubmit(_ctx.queues[_ctx.currentQueue].queue, 1, &submitInfo, cmd.fence);
    //DEBUG_VK(res, "Failed to submit command buffer");
    _ctx.EndCommandBuffer(submitInfo);

    auto res = vkQueuePresentKHR(_ctx.queues[_ctx.currentQueue].queue, &presentInfo);
    _ctx.currentQueue = Queue::Count;

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        _ctx.swapChainDirty = true;
        return;
    }
    else if (res != VK_SUCCESS) {
        DEBUG_VK(res, "Failed to present swap chain image!");
    }

    _ctx.swapChainCurrentFrame = (_ctx.swapChainCurrentFrame + 1) % _ctx.framesInFlight;
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
    vkCmdPipelineBarrier2(GetCurrentCommandResources().buffer, &dependency);
}

VkSampler Context::CreateSampler(f32 maxLod) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // todo: create separate one for shadow maps
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
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