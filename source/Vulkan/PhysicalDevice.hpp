#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class PhysicalDevice {
private:
    static inline std::vector<PhysicalDevice> allDevices;
    static inline PhysicalDevice*             device             = nullptr;
    static inline int                         index              = -1;
    static inline bool                        isDirty            = true;

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

    bool suitable       = false;
    int  presentFamily  = -1;
    int  graphicsFamily = -1;

    VkPhysicalDevice      vkDevice   = VK_NULL_HANDLE;
    VkSampleCountFlagBits maxSamples = VK_SAMPLE_COUNT_1_BIT;
    VkSampleCountFlags    sampleCounts;

    VkPhysicalDeviceFeatures         features{};
    VkSurfaceCapabilitiesKHR         capabilities{};
    VkPhysicalDeviceProperties       properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};

    std::vector<VkPresentModeKHR>        presentModes;
    std::vector<VkSurfaceFormatKHR>      surfaceFormats;
    std::vector<VkExtensionProperties>   extensions;
    std::vector<VkQueueFamilyProperties> families;

public:
    static void Create();
    static void Destroy();
    static void OnImgui();
    static void OnSurfaceUpdate();
    static void UpdateDevice();
    static uint32_t FindMemoryType(uint32_t type, VkMemoryPropertyFlags properties);
    static bool SupportFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);

    static inline bool                                      IsDirty()               { return isDirty;                }
    static inline VkPhysicalDevice                          GetVkPhysicalDevice()   { return device->vkDevice;       }
    static inline VkPhysicalDeviceFeatures                  GetFeatures()           { return device->features;       }
    static inline uint32_t                                  GetPresentFamily()      { return device->presentFamily;  }
    static inline uint32_t                                  GetGraphicsFamily()     { return device->graphicsFamily; }
    static inline VkSampleCountFlags                        GetSampleCounts()       { return device->sampleCounts;   }
    static inline VkSampleCountFlagBits                     GetMaxSamples()         { return device->maxSamples;     }
    static inline VkSurfaceCapabilitiesKHR                  GetCapabilities()       { return device->capabilities;   }
    static inline VkPhysicalDeviceProperties                GetProperties()         { return device->properties;     }
    static inline const std::vector<const char*>&           GetRequiredExtensions() { return requiredExtensions;     }
    static inline const std::vector<VkPresentModeKHR>&      GetPresentModes()       { return device->presentModes;   }
    static inline const std::vector<VkSurfaceFormatKHR>&    GetSurfaceFormats()     { return device->surfaceFormats; }
    static inline const std::vector<VkExtensionProperties>& GetExtensions()         { return device->extensions;     }
};