#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Image.hpp"

class SwapChain {
private:
    static inline VkSwapchainKHR               swapChain  = VK_NULL_HANDLE;
    static inline VkRenderPass                 renderPass = VK_NULL_HANDLE;
    static inline std::vector<VkImage>         images;
    static inline std::vector<VkImageView>     views;
    static inline std::vector<VkFramebuffer>   framebuffers;
    static inline std::vector<VkCommandBuffer> commandBuffers;
    static inline std::vector<VkSemaphore>     imageAvailableSemaphores;
    static inline std::vector<VkSemaphore>     renderFinishedSemaphores;
    static inline std::vector<VkFence>         inFlightFences;
    static inline std::vector<VkFence>         imagesInFlight;

    static inline ImageResource colorRes;
    static inline ImageResource depthRes;

    static inline uint32_t   additionalImages;
    static inline uint32_t   framesInFlight;
    static inline VkFormat   depthFormat;
    static inline VkExtent2D extent;
    static inline uint32_t   currentFrame;
    static inline int        newAdditionalImages = 1;
    static inline int        newFramesInFlight   = 3;
    static inline bool       dirty               = true;

    // preferred, warn if not available
    static inline VkFormat              colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    static inline VkColorSpaceKHR       colorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    static inline VkPresentModeKHR      presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    static inline VkSampleCountFlagBits numSamples  = VK_SAMPLE_COUNT_4_BIT;

    static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

public:
    static void Create();
    static void Destroy();
    static void OnImgui();
    static uint32_t Acquire();
    static void SubmitAndPresent(uint32_t imageIndex);

    static inline bool                  IsDirty()                    { return dirty;             }
    static inline VkExtent2D            GetExtent()                  { return extent;            }
    static inline uint32_t              GetNumFrames()               { return images.size();     }
    static inline uint32_t              GetFramesInFlight()          { return framesInFlight;    }
    static inline VkRenderPass          GetRenderPass()              { return renderPass;        }
    static inline VkSwapchainKHR        GetVkSwapChain()             { return swapChain;         }
    static inline VkSampleCountFlagBits GetNumSamples()              { return numSamples;        }
    static inline VkFramebuffer         GetFramebuffer(uint32_t i)   { return framebuffers[i];   }
    static inline VkCommandBuffer       GetCommandBuffer(uint32_t i) { return commandBuffers[i]; }
};