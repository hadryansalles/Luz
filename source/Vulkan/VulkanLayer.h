#pragma once

#include <memory>
#include <stdint.h>
#include <string>

struct GLFWwindow;

namespace vkw {

using Flags = uint32_t;

enum Memory {
    GPU = 0x00000001,
    CPU = 0x00000002 | 0x00000004,
};
using MemoryFlags = Flags;

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
    Address = 0x00020000,
    VideoDecodeSrc = 0x00002000,
    VideoDecodeDst = 0x00004000,
    TransformFeedback = 0x00000800,
    TransformFeedbackCounter = 0x00001000,
    ConditionalRendering = 0x00000200,
    AccelerationStructureInput = 0x00080000,
    AccelerationStructure = 0x00100000,
    ShaderBindingTable = 0x00000400,
    SamplerDescriptor = 0x00200000,
    ResourceDescriptor = 0x00400000,
    PushDescriptors = 0x04000000,
    MicromapBuildInputReadOnly = 0x00800000,
    MicromapStorage = 0x01000000,
};
using UsageFlags = Flags;

struct BufferResource;
struct CommandBufferResource;

struct Buffer {
    std::shared_ptr<BufferResource> resource;
    uint32_t size;
    UsageFlags usage;
    MemoryFlags memory;
    uint32_t rid;

// -------------------------------------- delete
    uint32_t StorageID();
    VkBuffer GetBuffer();
    void SetBuffer(VkBuffer vkBuffer, VkDeviceMemory vkMemory);
};

enum Queue {
    Graphics = 0,
    Compute = 1,
    Transfer = 2,
    Count = 3,
};

Buffer CreateBuffer(uint32_t size, UsageFlags usage, MemoryFlags memory = Memory::GPU, const std::string& name = "");

void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset = 0);
void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset = 0, uint32_t srcOffset = 0);

void BeginCommandBuffer(Queue queue);
uint64_t EndCommandBuffer(Queue queue);
void WaitQueue(Queue queue);

void Init(GLFWwindow* window, uint32_t width, uint32_t height);
void OnSurfaceUpdate(uint32_t width, uint32_t height);
void Destroy();

struct Context {
    void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset);
    void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset);

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    std::string applicationName = "Luz";
    std::string engineName = "Luz";

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

    struct CommandResources {
        u8* stagingCpu = nullptr;
        uint32_t stagingOffset = 0;
        Buffer staging;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer buffer = VK_NULL_HANDLE;
    };
    struct InternalQueue {
        VkQueue queue = VK_NULL_HANDLE;
        int family = -1;
        std::vector<CommandResources> commands;
    };
    InternalQueue queues[Queue::Count];
    Queue currentQueue = Queue::Graphics;
    const uint32_t stagingBufferSize = 64 * 1024 * 1024;

    // TODO: remove after finishing queue refactoring
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    int graphicsFamily = -1;

    VkPhysicalDeviceMemoryProperties memoryProperties;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainViews;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    uint32_t additionalImages = 0;
    uint32_t framesInFlight = 3;
    VkFormat depthFormat;
    VkExtent2D swapChainExtent;
    uint32_t swapChainCurrentFrame;
    bool swapChainDirty = true;
    int currentImageIndex = 0;

    uint32_t nextBufferRID = 32;

    // preferred, warn if not available
    static inline VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    static inline VkColorSpaceKHR colorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    static inline VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    static inline VkSampleCountFlagBits numSamples  = VK_SAMPLE_COUNT_1_BIT;

    Buffer stagingBuffer;
    uint32_t stagingBufferOffset = 0;

    void CreateInstance(GLFWwindow* window);
    void DestroyInstance();

    void CreatePhysicalDevice();

    void CreateDevice();
    void DestroyDevice();

    void CreateSurfaceFormats();

    void CreateSwapChain(uint32_t width, uint32_t height);
    void DestroySwapChain();

    uint32_t FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties);
    bool SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    uint32_t Acquire();
    void SubmitAndPresent(uint32_t imageIndex);

    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
};

// todo: move to private
Context& ctx();

}