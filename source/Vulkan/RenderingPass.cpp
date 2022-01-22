#include "Luzpch.hpp"
#include "RenderingPass.hpp"
#include "ImageManager.hpp"
#include "Texture.hpp"
#include "LogicalDevice.hpp"
#include "SwapChain.hpp"

void RenderingPassManager::Create() {
    sampler = CreateSampler(1);
}

void RenderingPassManager::Destroy() {
    vkDestroySampler(LogicalDevice::GetVkDevice(), sampler, nullptr);
    for (RID i = 0; i < nextRID; i++) {
        ImageManager::Destroy(imageAttachments[i]);
    }
    nextRID = 0;
}

void RenderingPassManager::CreateRenderingPass(RenderingPass& pass) {
    LUZ_PROFILE_FUNC();
    GraphicsPipelineManager::CreatePipeline(pass.gpoDesc, pass.gpo);
    VkExtent2D extent =  SwapChain::GetExtent();
    for(int i = 0; i < pass.gpoDesc.colorFormats.size(); i++) {
        ImageDesc desc;
        desc.width = extent.width;
        desc.height = extent.height;
        desc.mipLevels = 1;
        desc.format = pass.gpoDesc.colorFormats[i];
        desc.tiling = VK_IMAGE_TILING_OPTIMAL;
        desc.numSamples = SwapChain::GetNumSamples();
        desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        pass.colorAttachments.push_back(nextRID);
        ImageManager::Create(desc, imageAttachments[nextRID++]);
    }

    RID count = pass.colorAttachments.size();
    if (pass.gpoDesc.useDepthAttachment) {
        count += 1;
    }

    std::vector<VkDescriptorImageInfo> imageInfos(count);
    std::vector<VkWriteDescriptorSet> writes(count);

    VkDescriptorSet bindlessDescriptorSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
    DEBUG_ASSERT(bindlessDescriptorSet != VK_NULL_HANDLE, "Null bindless descriptor set!");

    pass.colorAttachInfos.resize(pass.colorAttachments.size());

    for (int i = 0; i < pass.colorAttachments.size(); i++) {
        RID rid = pass.colorAttachments[i];

        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = imageAttachments[rid].view;
        imageInfos[i].sampler = sampler;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = GraphicsPipelineManager::IMAGE_ATTACHMENT_BINDING;
        writes[i].dstArrayElement = rid;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];

        pass.colorAttachInfos[i] = {};
        pass.colorAttachInfos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        pass.colorAttachInfos[i].imageView = imageAttachments[rid].view;
        pass.colorAttachInfos[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pass.colorAttachInfos[i].resolveMode = VK_RESOLVE_MODE_NONE;
        pass.colorAttachInfos[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pass.colorAttachInfos[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass.colorAttachInfos[i].clearValue.color = pass.clearColors[i];
    }

    if (pass.gpoDesc.useDepthAttachment) {
        ImageDesc desc;
        desc.width = extent.width;
        desc.height = extent.height;
        desc.mipLevels = 1;
        desc.format = pass.gpoDesc.depthFormat;
        desc.tiling = VK_IMAGE_TILING_OPTIMAL;
        desc.numSamples = SwapChain::GetNumSamples();
        desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        desc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        pass.depthAttachment = nextRID;
        ImageManager::Create(desc, imageAttachments[nextRID++]);
        int i = pass.colorAttachments.size();
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = imageAttachments[pass.depthAttachment].view;
        imageInfos[i].sampler = sampler;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = bindlessDescriptorSet;
        writes[i].dstBinding = GraphicsPipelineManager::IMAGE_ATTACHMENT_BINDING;
        writes[i].dstArrayElement = pass.depthAttachment;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];

        pass.depthAttachInfo = {};
        pass.depthAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        pass.depthAttachInfo.imageView = imageAttachments[pass.depthAttachment].view;
        pass.depthAttachInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        pass.depthAttachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
        pass.depthAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pass.depthAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass.depthAttachInfo.clearValue.depthStencil = { 1.0f, 0 };
    }

    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), count, writes.data(), 0, nullptr);
}

void RenderingPassManager::DestroyRenderingPass(RenderingPass& pass) {
    GraphicsPipelineManager::DestroyPipeline(pass.gpo);
    pass.colorAttachments.clear();
    pass.depthAttachment = 0;
}