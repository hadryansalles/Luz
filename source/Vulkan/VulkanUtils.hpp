#pragma once

#include "Log.hpp"

static inline const char* LUZ_VkSampleCountFlagBitsStr(VkSampleCountFlagBits samples) {
    switch (samples) {
    case VK_SAMPLE_COUNT_1_BIT:
        return "VK_SAMPLE_COUNT_1_BIT";
    case VK_SAMPLE_COUNT_2_BIT:
        return "VK_SAMPLE_COUNT_2_BIT";
    case VK_SAMPLE_COUNT_4_BIT:
        return "VK_SAMPLE_COUNT_4_BIT";
    case VK_SAMPLE_COUNT_8_BIT:
        return "VK_SAMPLE_COUNT_8_BIT";
    case VK_SAMPLE_COUNT_16_BIT:
        return "VK_SAMPLE_COUNT_16_BIT";
    case VK_SAMPLE_COUNT_32_BIT:
        return "VK_SAMPLE_COUNT_32_BIT";
    case VK_SAMPLE_COUNT_64_BIT:
        return "VK_SAMPLE_COUNT_64_BIT";
    default:
        LOG_WARN("VkSampleCountFlagBitsStr reach unspecified condition with {}.", samples);
        return "Unspecified";
    }
}

static inline const char* LUZ_VkPresentModeKHRStr(VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_FIFO_KHR:
        return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "VK_PRESENT_MODE_MAILBOX_KHR";
    default:
        LOG_WARN("VkPresentModeKHRStr reach unspecified condition with {}.", mode);
        return "Unspecified";
    }
}

static inline const char* LUZ_VkFormatStr(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
        return "VK_FORMAT_B8G8R8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM:
        return "VK_FORMAT_B8G8R8A8_UNORM";
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return "VK_FORMAT_R16G16B16A16_SFLOAT";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
    default:
        LOG_WARN("VkFormatStr reach unspecified condition with {}.", format);
        return "Unspecified";
    }
}

static inline const char* LUZ_VkColorSpaceKHRStr(VkColorSpaceKHR space) {
    switch (space) {
    case VK_COLORSPACE_SRGB_NONLINEAR_KHR:
        return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
    default:
        LOG_WARN("VkColorSpaceKHRStr reach unspecified condition with {}.", space);
        return "Unspecified";
    }
}

static inline const char* LUZ_VkPolygonModeStr(VkPolygonMode mode) {
    switch (mode) {
    case VK_POLYGON_MODE_FILL:
        return "VK_POLYGON_MODE_FILL";
    case VK_POLYGON_MODE_LINE:
        return "VK_POLYGON_MODE_LINE";
    case VK_POLYGON_MODE_POINT:
        return "VK_POLYGON_MODE_POINT";
    default:
        LOG_WARN("VkPolygonMode reach unspecified condition with {}.", mode);
        return "Unspecified";
    }
}

constexpr std::array<VkPolygonMode, 3> LUZ_VkPolygonModes() {
    return {VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_POINT};
}

static inline const char* LUZ_VkCullModeStr(VkCullModeFlags mode) {
    switch (mode) {
    case VK_CULL_MODE_NONE:
        return "VK_CULL_MODE_NONE";
    case VK_CULL_MODE_BACK_BIT:
        return "VK_CULL_MODE_BACK_BIT";
    case VK_CULL_MODE_FRONT_BIT:
        return "VK_CULL_MODE_FRONT_BIT";
    case VK_CULL_MODE_FRONT_AND_BACK:
        return "VK_CULL_MODE_FRONT_AND_BACK";
    default:
        LOG_WARN("VkCullMode reach unspecified condition with {}.", mode);
        return "Unspecified";
    }
}

constexpr std::array<VkCullModeFlagBits, 4> LUZ_VkCullModes() {
    return {VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_FRONT_AND_BACK};
}