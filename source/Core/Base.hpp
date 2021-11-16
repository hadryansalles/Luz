#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <glm/glm.hpp>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using RID = u32;

#define ALIGN_AS(size, alignment) (size) % (alignment) > 0 ? (size) + (alignment) - (size) % (alignment) : (size)

#ifdef _WIN32
    #ifndef _WIN64
        #error "x86 not supported!"
    #endif

	#define LUZ_PLATFORM_WINDOWS
#endif

#ifdef __linux__
	#define LUZ_PLATFORM_LINUX
#endif

#ifdef LUZ_DEBUG
	#ifdef LUZ_PLATFORM_WINDOWS
		#define LUZ_DEBUGBREAK() __debugbreak()
	#endif

	#ifdef LUZ_PLATFORM_LINUX
        #include <csignal>
		#define LUZ_DEBUGBREAK() raise(SIGTRAP)
	#endif
#else
	#define LUZ_DEBUGBREAK()
#endif 

#define ASSERT(condition, ...) { if (!(condition)) { LOG_ERROR("[ASSERTION FAILED] {0}", __VA_ARGS__); abort(); } }

#ifdef LUZ_DEBUG
#define DEBUG_ASSERT(condition, ...) { if (!(condition)) { LOG_ERROR("[ASSERTION FAILED] {0}", __VA_ARGS__); LUZ_DEBUGBREAK(); } }
#define DEBUG_VK(res, ...) { if ((res) != VK_SUCCESS) { LOG_ERROR("[VULKAN ERROR = {0}] {1}", VK_ERROR_STRING((res)), __VA_ARGS__); LUZ_DEBUGBREAK(); } }
#else
#define DEBUG_ASSERT(condition, ...)
#define DEBUG_VK(res, ...)
#endif

// taken from Sam Lantiga: https://www.libsdl.org/tmp/SDL/test/testvulkan.c
static const char *VK_ERROR_STRING(VkResult result) {
    switch((int)result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
        return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    default:
        break;
    }
    if(result < 0)
        return "VK_ERROR_<Unknown>";
    return "VK_<Unknown>";
}
