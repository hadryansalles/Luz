#include "Luzpch.hpp"

#include "SwapChain.hpp"
#include "LogicalDevice.hpp"
#include "PhysicalDevice.hpp"
#include "Window.hpp"
#include "Instance.hpp"
#include "VulkanUtils.h"

void SwapChain::Create() {
    auto device = LogicalDevice::GetVkDevice();
    auto instance = Instance::GetVkInstance();
    auto allocator = Instance::GetAllocator();

    if (numSamples > PhysicalDevice::GetMaxSamples()) {
        numSamples = PhysicalDevice::GetMaxSamples();
    }

    // create swapchain
    {
        const auto& capabilities = PhysicalDevice::GetCapabilities();
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(PhysicalDevice::GetSurfaceFormats());
        colorFormat = surfaceFormat.format;
        colorSpace = surfaceFormat.colorSpace;
        presentMode = ChoosePresentMode(PhysicalDevice::GetPresentModes());
        extent = ChooseExtent(capabilities);

        framesInFlight = newFramesInFlight;
        additionalImages = newAdditionalImages;

        // acquire additional images to prevent waiting for driver's internal operations
        uint32_t imageCount = framesInFlight + additionalImages;
        
        if (imageCount < capabilities.minImageCount) {
            LOG_ERROR("Querying less images than the necessary!");
            imageCount = capabilities.minImageCount;
        }

        // prevent exceeding the max image count
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            LOG_WARN("Querying more images than supported. imageCoun set to maxImageCount.");
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = Instance::GetVkSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // amount of layers each image consist of
        // it's 1 unless we are developing some 3D stereoscopic app
        createInfo.imageArrayLayers = 1;
        // if we want to render to a separate image first to perform post-processing
        // we should change this image usage
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = { PhysicalDevice::GetGraphicsFamily(), PhysicalDevice::GetPresentFamily() };

        // if the graphics family is different thant the present family
        // we need to handle how the images on the swap chain will be accessed by the queues
        if (PhysicalDevice::GetGraphicsFamily() != PhysicalDevice::GetPresentFamily()) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

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
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());
    }

    // create image views
    views.resize(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        auto res = vkCreateImageView(device, &viewInfo, allocator, &views[i]);
        DEBUG_VK(res, "Failed to create SwapChain image view!");
    }

    // create resources
    {
        // find format for depth resource
        auto candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
        auto optimalTiling = VK_IMAGE_TILING_OPTIMAL;
        auto depthFeature = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        bool validFormat = false;
        for (auto candidate : candidates) {
            if (PhysicalDevice::SupportFormat(candidate, optimalTiling, depthFeature)) {
                depthFormat = candidate;
                validFormat = true;
                break;
            }
        }
        ASSERT(validFormat, "Failed to find valid format for depth resource!");

        ImageDesc buffersDesc;
        buffersDesc.width = extent.width;
        buffersDesc.height = extent.height;
        buffersDesc.mipLevels = 1;
        buffersDesc.format = depthFormat;
        buffersDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        buffersDesc.numSamples = SwapChain::GetNumSamples();
        buffersDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        buffersDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        buffersDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        Image::Create(buffersDesc, depthRes);

        buffersDesc.format = colorFormat;
        buffersDesc.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        buffersDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

        Image::Create(buffersDesc, colorRes);
    }

    // create render pass
    {
        std::vector<VkAttachmentDescription> attachments;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = numSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments.push_back(colorAttachment);

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = numSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments.push_back(depthAttachment);
        
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkAttachmentDescription colorAttachmentResolve{};
        VkAttachmentReference colorAttachmentResolveRef{};
        if (numSamples > 1) {
            colorAttachmentResolve.format = colorFormat;
            colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            colorAttachmentResolveRef.attachment = 2;
            colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            subpass.pResolveAttachments = &colorAttachmentResolveRef;

            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(colorAttachmentResolve);
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        auto res = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
        DEBUG_VK(res, "Failed to create render pass!");
        // VkAttachmentDescription colorAttachment{};
        // colorAttachment.format = colorFormat;
        // colorAttachment.samples = numSamples;
        // colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // VkAttachmentDescription depthAttachment{};
        // depthAttachment.format = depthFormat;
        // depthAttachment.samples = numSamples;
        // depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // VkAttachmentDescription colorAttachmentResolve{};
        // colorAttachmentResolve.format = colorFormat;
        // colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        // colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // VkAttachmentReference colorAttachmentRef{};
        // colorAttachmentRef.attachment = 0;
        // colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // VkAttachmentReference depthAttachmentRef{};
        // depthAttachmentRef.attachment = 1;
        // depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // VkAttachmentReference colorAttachmentResolveRef{};
        // colorAttachmentResolveRef.attachment = 2;
        // colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // VkSubpassDescription subpass{};
        // subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // subpass.colorAttachmentCount = 1;
        // subpass.pColorAttachments = &colorAttachmentRef;
        // subpass.pDepthStencilAttachment = &depthAttachmentRef;
        // subpass.pResolveAttachments = &colorAttachmentResolveRef;

        // VkSubpassDependency dependency{};
        // dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        // dependency.dstSubpass = 0;
        // dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // dependency.srcAccessMask = 0;
        // dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve };
        // VkRenderPassCreateInfo renderPassInfo{};
        // renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        // renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        // renderPassInfo.pAttachments = attachments.data();
        // renderPassInfo.subpassCount = 1;
        // renderPassInfo.pSubpasses = &subpass;
        // renderPassInfo.dependencyCount = 1;
        // renderPassInfo.pDependencies = &dependency;

        // if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to create render pass!");
        // }
    }

    // create framebuffers
    {
        framebuffers.resize(images.size());
        for (size_t i = 0; i < images.size(); i++) {
            std::vector<VkImageView> attachments; 
            if (numSamples > 1) {
                attachments.push_back(colorRes.view);
                attachments.push_back(depthRes.view);
                attachments.push_back(views[i]);
            }
            else {
                attachments.push_back(views[i]);
                attachments.push_back(depthRes.view);
            }

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = attachments.size();
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            auto res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]);
            DEBUG_VK(res, "Failed to create framebuffer!");
        }
        // framebuffers.resize(images.size());

        // for (size_t i = 0; i < views.size(); i++) {
        //     std::array<VkImageView, 3> attachments = {
        //         colorRes.view,
        //         depthRes.view,
        //         views[i]
        //     };

        //     VkFramebufferCreateInfo framebufferInfo{};
        //     framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        //     framebufferInfo.renderPass = renderPass;
        //     framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        //     framebufferInfo.pAttachments = attachments.data();
        //     framebufferInfo.width = extent.width;
        //     framebufferInfo.height = extent.height;
        //     framebufferInfo.layers = 1;

        //     if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
        //         throw std::runtime_error("failed to create framebuffer!");
        //     }
        // }
    }

    LOG_INFO("Create Swapchain");
    dirty = false;
}

void SwapChain::Destroy() {
    auto device = LogicalDevice::GetVkDevice();
    auto allocator = Instance::GetAllocator();

    Image::Destroy(colorRes);
    Image::Destroy(depthRes);

    for (size_t i = 0; i < images.size(); i++) {
        vkDestroyFramebuffer(device, framebuffers[i], allocator);
        vkDestroyImageView(device, views[i], allocator);
    }

    vkDestroyRenderPass(device, renderPass, allocator);
    vkDestroySwapchainKHR(device, swapChain, allocator);

    framebuffers.clear();
    views.clear();
    images.clear();
    swapChain = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
}

void SwapChain::OnImgui() {
    const float totalWidth = ImGui::GetContentRegionAvailWidth();
    const auto cap = PhysicalDevice::GetCapabilities();
    if (ImGui::CollapsingHeader("SwapChain")) {
        // Frames in Flight
        {
            ImGui::Text("Frames In Flight");
            ImGui::SameLine(totalWidth*3.0/5.0f);
            ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
            ImGui::PushID("framesInFlight");
            newFramesInFlight = framesInFlight;
            if (ImGui::InputInt("", &newFramesInFlight)) {
                if (newFramesInFlight != framesInFlight) {
                    dirty = true;
                }
                if (newFramesInFlight < 1) {
                    newFramesInFlight = 1;
                }
                else if (newFramesInFlight + additionalImages < cap.minImageCount) {
                    newFramesInFlight = cap.minImageCount - additionalImages;
                }
                else if (cap.maxImageCount > 0 && newFramesInFlight + additionalImages > cap.maxImageCount) {
                    newFramesInFlight = cap.maxImageCount - additionalImages;
                }
                else {
                    framesInFlight = newFramesInFlight;
                }
            }
            ImGui::PopID();
        }
        // Additional SwapChain images
        {
            ImGui::Text("Additional SwapChain Images");
            ImGui::SameLine(totalWidth*3.0/5.0f);
            ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
            ImGui::PushID("additionalSwapChainImages");
            newAdditionalImages = additionalImages;
            if (ImGui::InputInt("", &newAdditionalImages)) {
                if (newAdditionalImages != additionalImages) {
                    dirty = true;
                }
                if (newAdditionalImages + framesInFlight < cap.minImageCount || newAdditionalImages < 0) {
                    newAdditionalImages = cap.minImageCount - framesInFlight;
                }
                else if (cap.maxImageCount > 0 && newAdditionalImages + framesInFlight > cap.maxImageCount) {
                    newAdditionalImages = cap.maxImageCount - framesInFlight;
                }
                else {
                    additionalImages = newAdditionalImages;
                }
            }
            ImGui::PopID();
        }
        // Present Mode
        {
            ImGui::Text("Present Mode");
            ImGui::SameLine(totalWidth*3.0/5.0f);
            ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
            ImGui::PushID("presentMode");
            if (ImGui::BeginCombo("", VkPresentModeKHRStr(presentMode))) {
                for (auto mode : PhysicalDevice::GetPresentModes()) {
                    bool selected = mode == presentMode;
                    if (ImGui::Selectable(VkPresentModeKHRStr(mode), selected) && !selected) {
                        presentMode = mode;
                        dirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        // Num Samples
        {
            ImGui::Text("Num Samples");
            ImGui::SameLine(totalWidth*3.0/5.0f);
            ImGui::SetNextItemWidth(totalWidth*2.0/5.0f);
            ImGui::PushID("samplesCombo");
            if (ImGui::BeginCombo("", VkSampleCountFlagBitsStr(numSamples))) {
                for (size_t i = 1; i <= PhysicalDevice::GetMaxSamples(); i *= 2) {
                    VkSampleCountFlagBits curSamples = (VkSampleCountFlagBits)i;
                    bool selected = curSamples == numSamples;
                    if (ImGui::Selectable(VkSampleCountFlagBitsStr(curSamples), selected) && !selected) {
                        numSamples = curSamples;
                        dirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        // Surface Format
        {
            if (ImGui::TreeNode("Surface Format")) {
                auto surfaceFormats = PhysicalDevice::GetSurfaceFormats();
                for (size_t i = 0; i < surfaceFormats.size(); i++) {
                    const char* formatName = VkFormatStr(surfaceFormats[i].format);
                    const char* spaceName = VkColorSpaceKHRStr(surfaceFormats[i].colorSpace);
                    bool selected = (surfaceFormats[i].colorSpace == colorSpace && surfaceFormats[i].format == colorFormat);
                    ImGui::PushID(i);
                    if (ImGui::RadioButton("", selected)) {
                        colorFormat = surfaceFormats[i].format;
                        colorSpace = surfaceFormats[i].colorSpace;
                        dirty = true;
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s, %s", formatName, spaceName);
                }
                ImGui::TreePop();
            }
        }
    }
}

VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == colorFormat
            && availableFormat.colorSpace == colorSpace) {
            return availableFormat;
        }
    }
    LOG_WARN("Preferred surface format not available!");
    return formats[0];
}

VkPresentModeKHR SwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
    for (const auto& mode : presentModes) {
        if (mode == presentMode) {
            return mode;
        }
    }
    LOG_WARN("Preferred present mode not available!");
    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { Window::GetWidth(), Window::GetHeight() };

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
