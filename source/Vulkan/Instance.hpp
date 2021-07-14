#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class Instance {
private:
    static inline VkInstance               instance       = VK_NULL_HANDLE;
    static inline VkSurfaceKHR             surface        = VK_NULL_HANDLE;
    static inline VkAllocationCallbacks*   allocator      = VK_NULL_HANDLE;
    static inline VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    static inline std::string applicationName = "Luz";
    static inline std::string engineName      = "Vulkan Rendering Engine";

    static inline uint32_t                           apiVersion;
    static inline std::vector<bool>                  activeValidationLayers;
    static inline std::vector<const char*>           activeValidationLayersNames;
    static inline std::vector<VkLayerProperties>     validationLayers;
    static inline std::vector<bool>                  activeExtensions;
    static inline std::vector<const char*>           activeExtensionsNames;
    static inline std::vector<VkExtensionProperties> extensions;

    static inline bool enableValidationLayers = true;
    static inline bool dirty                  = true;

public:
    static void Create();
    static void Destroy();
    static void OnImgui();

    static inline VkInstance                      GetVkInstance()             { return instance;                    }
    static inline VkSurfaceKHR                    GetVkSurface()              { return surface;                     }
    static inline VkAllocationCallbacks*          GetAllocator()              { return allocator;                   }
    static inline bool                            IsDirty()                   { return dirty;                       }
    static inline bool                            IsValidationLayersEnabled() { return enableValidationLayers;      }
    static inline const std::vector<const char*>& GetValidationLayers()       { return activeValidationLayersNames; }
};
