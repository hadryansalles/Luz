#pragma once

#include <memory>
#include <stdint.h>
#include <string>
#include <filesystem>

struct GLFWwindow;

namespace vkw {

using Flags = uint32_t;

enum Memory {
    GPU = 0x00000001,
    CPU = 0x00000002 | 0x00000004,
};
using MemoryFlags = Flags;

namespace BufferUsage {
    enum {
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
}
using BufferUsageFlags = Flags;

enum Format {
    RGBA8_unorm = 37,
    BGRA8_unorm = 44,
    RG32_sfloat = 103,
    RGB32_sfloat = 106,
    RGBA32_sfloat = 109,
    D32_sfloat = 126,
    D24_unorm_S8_uint = 129,
};

namespace ImageUsage {
    enum {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthAttachment = 0x00000020,
    };
}
using ImageUsageFlags = Flags;

namespace Aspect {
    enum {
        Color = 1,
        Depth = 2,
        Stencil = 4,
    };
}
using AspectFlags = Flags;

namespace Layout {
    enum ImageLayout {
        Undefined = 0,
        General = 1,
        ColorAttachment = 2,
        DepthStencilAttachment = 3,
        DepthStencilRead = 4,
        ShaderRead = 5,
        TransferSrc = 6,
        TransferDst = 7,
        DepthReadStencilAttachment = 1000117000,
        DepthAttachmentStencilRead = 1000117001,
        DepthAttachment = 1000241000,
        DepthRead = 1000241001,
        StencilAttachment = 1000241002,
        StencilRead = 1000241003,
        Read = 1000314000,
        Attachment = 1000314001,
        Present = 1000001002,
    };
}

struct BufferResource;
struct ImageResource;
struct PipelineResource;
struct TLASResource;
struct BLASResource;

struct Buffer {
    std::shared_ptr<BufferResource> resource;
    uint32_t size;
    BufferUsageFlags usage;
    MemoryFlags memory;
    uint32_t rid;
// -------------------------------------- todo: delete
    VkBuffer GetBuffer();
};

struct Image {
    std::shared_ptr<ImageResource> resource;
    uint32_t width = 0;
    uint32_t height = 0;
    ImageUsageFlags usage;
    Format format;
    Layout::ImageLayout layout;
    AspectFlags aspect;
    uint32_t rid;
    ImTextureID imguiRID;

    // todo: remove
    VkImageView GetView();
    VkImage GetImage();
};

enum Queue {
    Graphics = 0,
    Compute = 1,
    Transfer = 2,
    Count = 3,
};

struct ImageDesc {
    uint32_t width;
    uint32_t height;
    Format format;
    ImageUsageFlags usage;
    std::string name = "";
};

namespace PipelinePoint {
    enum Point {
        Graphics = 0,
        Compute = 1,
    };
}

namespace ShaderStage {
    enum Stage {
        Vertex = 0x00000001,
        Geometry = 0x00000008,
        Fragment = 0x00000010,
        Compute = 0x00000020,
        AllGraphics = 0x0000001F,
        All = 0x7FFFFFFF,
    };
}

struct Pipeline {
    struct Stage {
        ShaderStage::Stage stage;
        std::filesystem::path path;
    };
    PipelinePoint::Point point;
    std::shared_ptr<PipelineResource> resource;
    std::vector<Stage> stages;
    std::vector<std::vector<char>> stageBytes;
};

struct PipelineDesc {
    PipelinePoint::Point point;
    std::vector<Pipeline::Stage> stages;
    glm::ivec2 extent = { 0, 0 };
    std::string name = "";
    std::vector<Format> vertexAttributes;
    std::vector<Format> colorFormats;
    bool useDepth = false;
    Format depthFormat;
};


struct BLAS {
    std::shared_ptr<BLASResource> resource;
};

struct BLASInstance {
    BLAS blas;
    glm::mat4 modelMat;
    uint32_t customIndex;
};

struct BLASDesc {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexStride;
    std::string name;
};

struct TLAS {
    std::shared_ptr<TLASResource> resource;
    uint32_t rid;
};

Buffer CreateBuffer(uint32_t size, BufferUsageFlags usage, MemoryFlags memory = Memory::GPU, const std::string& name = "");
Image CreateImage(const ImageDesc& desc);
Pipeline CreatePipeline(const PipelineDesc& desc);
TLAS CreateTLAS(uint32_t maxInstances, const std::string& name);
BLAS CreateBLAS(const BLASDesc& desc);

void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset = 0);
void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset = 0, uint32_t srcOffset = 0);
void CmdCopy(Image& dst, void* data, uint32_t size);
void CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset = 0);
void CmdBarrier(Image& img, Layout::ImageLayout layout);
void CmdBarrier();
void CmdBeginRendering(const std::vector<Image>& colorAttachs, Image depthAttach = {}, const glm::ivec2& extent = { 0, 0 }, const glm::ivec2& offset = { 0, 0 });
void CmdEndRendering();
void CmdBindPipeline(Pipeline& pipeline);
void CmdPushConstants(void* data, uint32_t size);
void CmdBuildBLAS(BLAS& blas);
void CmdBuildTLAS(TLAS& tlas, const std::vector<BLASInstance>& instances);

void BeginCommandBuffer(Queue queue);
uint64_t EndCommandBuffer();
void WaitQueue(Queue queue);

void Init(GLFWwindow* window, uint32_t width, uint32_t height);
void OnSurfaceUpdate(uint32_t width, uint32_t height);
void Destroy();

struct Context {
    void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset);
    void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset, uint32_t srcOffset);
    void CmdCopy(Image& dst, void* data, uint32_t size);
    void CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset);
    void CmdBarrier(Image& img, Layout::ImageLayout layout);
    void CmdBarrier();
    void CmdBeginRendering();
    void CmdEndRendering();

    void LoadShaders(Pipeline& pipeline);
    std::vector<char> CompileShader(const std::filesystem::path& path);
    void CreatePipeline(const PipelineDesc& desc, Pipeline& pipeline);

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

    struct CommandResources {
        u8* stagingCpu = nullptr;
        uint32_t stagingOffset = 0;
        Buffer staging;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    struct InternalQueue {
        VkQueue queue = VK_NULL_HANDLE;
        int family = -1;
        std::vector<CommandResources> commands;
    };
    InternalQueue queues[Queue::Count];
    Queue currentQueue = Queue::Count;
    std::shared_ptr<PipelineResource> currentPipeline;
    const uint32_t stagingBufferSize = 64 * 1024 * 1024;

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
    int currentImageIndex = 0;

    uint32_t nextBufferRID = 0;
    uint32_t nextImageRID = 0;
    uint32_t nextTLASRID = 1;
    VkSampler genericSampler;

    // preferred, warn if not available
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    VkSampleCountFlagBits numSamples  = VK_SAMPLE_COUNT_1_BIT;

    const uint32_t initialScratchBufferSize = 64*1024*1024;
    Buffer asScratchBuffer;
    VkDeviceAddress asScratchAddress;

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

    uint32_t Acquire();
    void SubmitAndPresent(uint32_t imageIndex);

    inline VkCommandBuffer GetCurrentCommandBuffer() {
        return queues[currentQueue].commands[currentImageIndex].buffer;
    }
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
};

// todo: move to private
Context& ctx();

}