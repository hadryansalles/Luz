#include "Luzpch.hpp"

#include "AssetManager.hpp"
#include "BufferManager.hpp"
#include "TextureManager.hpp"
#include "PhysicalDevice.hpp"
#include "LogicalDevice.hpp"
#include "Instance.hpp"
#include "GraphicsPipelineManager.hpp"
#include "UnlitGraphicsPipeline.hpp"
#include "PhongGraphicsPipeline.hpp"
#include "SwapChain.hpp"

#include "imgui/imgui_impl_vulkan.h"
#include <stb_image.h>

void TextureManager::Setup() {
    resources[0].path = "assets/default.png";
    resources[1].path = "assets/white.png";
    nextRID = 2;
}

void TextureManager::Create() {
    LUZ_PROFILE_FUNC();

    for (int i = 0; i < nextRID; i++) {
        TextureDesc desc = AssetManager::LoadTexture(resources[i].path);
        DEBUG_ASSERT(desc.data != nullptr, "Image '{}' not loaded. Can't create texture for it!", desc.path.string().c_str());
        InitTexture(i, desc);
    }

    UpdateDescriptor(0, nextRID);
}

void TextureManager::Destroy() {
    for (int i = 0; i < nextRID; i++) {
        ImageManager::Destroy(resources[i].image);
        vkDestroySampler(LogicalDevice::GetVkDevice(), resources[i].sampler, Instance::GetAllocator());
        resources[i].sampler = VK_NULL_HANDLE;
        resources[i].imguiRID = nullptr;
    }
}

void TextureManager::Finish() {
    for (int i = 0; i < nextRID; i++) {
        DEBUG_ASSERT(resources[i].imguiRID == nullptr, "Finish texture manager without destroying textures!");
        DEBUG_ASSERT(resources[i].sampler == nullptr, "Finish texture manager without destroying textures!");
        resources[i].path = "Invalid";
    }
    nextRID = 0;
}

VkSampler TextureManager::CreateSampler(f32 maxLod) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    // get the max ansotropy level of my device
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(PhysicalDevice::GetVkPhysicalDevice(), &properties);

    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

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
    auto vkRes = vkCreateSampler(LogicalDevice::GetVkDevice(), &samplerInfo, nullptr, &sampler);
    DEBUG_VK(vkRes, "Failed to create texture sampler!");

    return sampler;
}

void TextureManager::InitTexture(RID rid, TextureDesc& desc) {
    TextureResource& res = resources[rid];
    res.path = desc.path;
    u32 mipLevels = (u32)(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
    ImageManager::Create(desc.data, desc.width, desc.height, 4, mipLevels, res.image);
    stbi_image_free(desc.data);
    res.sampler = CreateSampler(mipLevels);
}

RID TextureManager::CreateTexture(TextureDesc& desc) {
    if (desc.path == "") {
        return 0;
    }

    if (nextRID >= MAX_TEXTURES) {
        LOG_ERROR("Can't create new texture! Max number of textures exceed!");
        return 0;
    }

    InitTexture(nextRID, desc);
    UpdateDescriptor(nextRID, nextRID + 1);

    return nextRID++;
}

void TextureManager::OnImgui() {
    if(ImGui::CollapsingHeader("Textures")) {
        ImGui::PushID("texture scale");
        ImGui::SliderFloat("", &imguiTextureScale, 0.01f, 10.0f);
        ImGui::PopID();
        for (int i = 0; i < nextRID; i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(resources[i].path.stem().string().c_str())) {
                ImVec2 size = ImVec2(resources[i].image.width, resources[i].image.height);
                size = ImVec2(size.x * imguiTextureScale, size.y * imguiTextureScale);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    std::string texturePath = resources[i].path.string();
                    ImGui::SetDragDropPayload("texture", texturePath.c_str(), texturePath.size());
                    ImGui::Image(resources[i].imguiRID, size);
                    ImGui::EndDragDropSource();
                }
                ImGui::Image(resources[i].imguiRID, size);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    std::string texturePath = resources[i].path.string();
                    ImGui::SetDragDropPayload("texture", texturePath.c_str(), texturePath.size());
                    ImGui::Image(resources[i].imguiRID, size);
                    ImGui::EndDragDropSource();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

void TextureManager::UpdateDescriptor(RID first, RID last) {
    LUZ_PROFILE_FUNC();
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const RID count = last - first;
    std::vector<VkDescriptorImageInfo> imageInfos(count);
    std::vector<VkWriteDescriptorSet> writes(count);

    VkDescriptorSet bindlessDescriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    DEBUG_ASSERT(bindlessDescriptorSet != VK_NULL_HANDLE, "Null bindless descriptor set!");

    for (RID i = 0; i < count; i++) {
        DEBUG_ASSERT(resources[i].sampler != VK_NULL_HANDLE, "Texture not loaded!");
        RID rid = first + i;
        resources[rid].imguiRID = ImGui_ImplVulkan_AddTexture(resources[rid].sampler, resources[rid].image.view, layout);

        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = resources[rid].image.view;
        imageInfos[i].sampler = resources[rid].sampler;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = 0;
        writes[i].dstArrayElement = rid;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }
    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), count, writes.data(), 0, nullptr);
}

void TextureManager::DrawOnImgui(RID rid) {
    DEBUG_ASSERT(rid < nextRID, "Trying to draw invalid texture {}", rid);
    float hSpace = ImGui::GetContentRegionAvailWidth()/2.5f;
    f32 maxSize = std::max(resources[rid].image.width, resources[rid].image.height);
    ImVec2 size = ImVec2((f32)resources[rid].image.width/maxSize, (f32)resources[rid].image.height/maxSize);
    size = ImVec2(size.x*hSpace, size.y * hSpace);
    ImGui::Image(resources[rid].imguiRID, size);
}

RID TextureManager::GetTexture(std::filesystem::path path) {
    LUZ_PROFILE_FUNC();
    for (RID i = 0; i < nextRID; i++) {
        if (path == resources[i].path) {
            return i;
        }
    }
    return 0;
}
