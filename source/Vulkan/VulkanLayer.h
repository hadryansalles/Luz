#pragma once

#include <memory>
#include <stdint.h>
#include <string>

struct GLFWwindow;

namespace vkw {

enum class Memory {
    GPU = 0x00000001,
    CPU = 0x00000002 | 0x00000004,
};

enum Usage {
    TransferSrc = 0x00000001,
    TransferDst = 0x00000002,
    UniformTexel = 0x00000004,
    StorageTexel = 0x00000008,
    Uniform = 0x00000010,
    Storage = 0x00000020,
    Index = 0x00000040,
    Vertex = 0x00000080,
    Indirect = 0x00000100,
    ShaderDeviceAdress = 0x00020000,
    VideoDecodeSrc = 0x00002000,
    VideoDecodeDst = 0x00004000,
    TransformFeedback = 0x00000800,
    TransformFeedbackCounter = 0x00001000,
    ConditionalRendering = 0x00000200,
    AccelerationStructureBuildInputReadOnly = 0x00080000,
    AccelerationStructureStorage = 0x00100000,
    ShaderBindingTable = 0x00000400,
    SamplerDescriptor = 0x00200000,
    ResourceDescriptor = 0x00400000,
    PushDescriptors = 0x04000000,
    MicromapBuildInputReadOnly = 0x00800000,
    MicromapStorage = 0x01000000,
};

struct BufferResource;

struct Buffer {
    std::shared_ptr<BufferResource> resource;
    const uint32_t size;
    const Usage usage;
    const Memory memory;
    uint32_t RID();
};

Buffer CreateBuffer(uint32_t size, Usage usage, Memory memory, const std::string& name = "");

void Init(GLFWwindow* window);
void Destroy();
void* Map(Buffer buffer);
void Unmap(Buffer buffer);

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    std::string applicationName = "Luz";
    std::string engineName = "Vulkan Rendering Engine";

    uint32_t apiVersion;
    std::vector<bool> activeLayers;
    std::vector<const char*> activeLayersNames;
    std::vector<VkLayerProperties> layers;
    std::vector<bool> activeExtensions;
    std::vector<const char*> activeExtensionsNames;
    std::vector<VkExtensionProperties> instanceExtensions;

    bool enableValidationLayers = true;

    static inline std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    };

    int presentFamily = -1;
    int graphicsFamily = -1;

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

    VkDevice device = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    void CreateInstance(GLFWwindow* window);
    void CreatePhysicalDevice();
    void CreateDevice();
    void OnSurfaceUpdate();
    void DestroyInstance();
    void DestroyDevice();
    uint32_t FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties);
    bool SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
};
// todo: move to private
Context& ctx();

}