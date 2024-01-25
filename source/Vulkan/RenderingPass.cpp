#include "Luzpch.hpp"
#include "RenderingPass.hpp"
#include "ImageManager.hpp"
#include "Texture.hpp"
#include "VulkanLayer.h"

void RenderingPassManager::Create() {
}

void RenderingPassManager::Destroy() {
}

void RenderingPassManager::CreateRenderingPass(RenderingPass& pass) {
    LUZ_PROFILE_FUNC();
    GraphicsPipelineManager::CreatePipeline(pass.gpoDesc, pass.gpo);
    VkExtent2D extent =  vkw::ctx().swapChainExtent;

    if (pass.createAttachments) {
        for (int i = 0; i < pass.gpoDesc.colorFormats.size(); i++) {
            vkw::Image attach = vkw::CreateImage({
                .width = extent.width,
                .height = extent.height,
                .format = (vkw::Format)pass.gpoDesc.colorFormats[i],
                .usage = vkw::ImageUsage::ColorAttachment | vkw::ImageUsage::Sampled,
                .name = "Color Attachment " + pass.gpoDesc.name + " " + std::to_string(i) ,
            });
            pass.colorAttachments.push_back(attach);
        }

        pass.colorAttachInfos.resize(pass.colorAttachments.size());

        for (int i = 0; i < pass.colorAttachments.size(); i++) {
            pass.colorAttachInfos[i] = {};
            pass.colorAttachInfos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            pass.colorAttachInfos[i].imageView = pass.colorAttachments[i].GetView();
            pass.colorAttachInfos[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass.colorAttachInfos[i].resolveMode = VK_RESOLVE_MODE_NONE;
            pass.colorAttachInfos[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass.colorAttachInfos[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass.colorAttachInfos[i].clearValue.color = pass.clearColors[i];
        }

        if (pass.gpoDesc.useDepthAttachment) {
            pass.depthAttachment = vkw::CreateImage({
                .width = extent.width,
                .height = extent.height,
                .format = (vkw::Format)pass.gpoDesc.depthFormat,
                .usage = vkw::ImageUsage::DepthAttachment | vkw::ImageUsage::Sampled,
                .name = "Depth image " + pass.gpoDesc.name,
            });
            pass.depthAttachInfo = {};
            pass.depthAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            pass.depthAttachInfo.imageView = pass.depthAttachment.GetView();
            pass.depthAttachInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            pass.depthAttachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            pass.depthAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass.depthAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass.depthAttachInfo.clearValue.depthStencil = { 1.0f, 0 };
        }
    }
}

void RenderingPassManager::DestroyRenderingPass(RenderingPass& pass) {
    GraphicsPipelineManager::DestroyPipeline(pass.gpo);
    pass.colorAttachments.clear();
    pass.depthAttachment = {};
}