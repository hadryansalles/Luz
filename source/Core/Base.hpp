#pragma once

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
using UUID = u64;

template<typename T>
using Ref = std::shared_ptr<T>;

#define ALIGN_AS(size, alignment) ((size) % (alignment) > 0 ? (size) + (alignment) - (size) % (alignment) : (size))
#define COUNT_OF(arr) (sizeof((arr)) / sizeof((arr)[0]))

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
