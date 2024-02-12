cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

# bin/lib output config
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

# set default output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# set default build types
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

# set build-specific output directories
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib/${OUTPUTCONFIG})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/bin/${OUTPUTCONFIG})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

if(WIN32)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        add_compile_options("/MP")
    endif()
endif()

# project config
project(Luz)

# define LUZ_DEBUG if is debug build
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_compile_definitions(LUZ_DEBUG)
endif()
# define LUZ_RELEASE if is release build
if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_compile_definitions(LUZ_RELEASE)
endif()

# config relative lib folder
set(LIBS_DIR ${CMAKE_SOURCE_DIR}/lib/${CMAKE_CFG_INTDIR})

# Luz
file(GLOB_RECURSE GLSL_SOURCE "source/*.vert" "source/*.frag" "source/*.rchit" "source/*.rgen" "source/*.rmiss")
file(GLOB_RECURSE SOURCE "source/*.cpp" "source/*.hpp" "source/*.h")
file(GLOB HEADERS "source/*")
add_executable(${PROJECT_NAME} ${SOURCE} ${GLSL_SOURCE})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE} ${GLSL_SOURCE})
include_directories(${HEADERS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_precompile_headers(${PROJECT_NAME} PRIVATE "source/Core/Luzpch.hpp")

# glfw
add_subdirectory("deps/glfw")
include_directories("deps/glfw/include")
target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES})

# vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

# glm
include_directories("deps/glm")

# spdlog
include_directories("deps/spdlog")

# optick
include_directories("deps/optick")
file(GLOB_RECURSE OPTICK_SOURCE "deps/optick/*.cpp")
add_library(optick ${OPTICK_SOURCE})
target_link_libraries(${PROJECT_NAME} optick)

# ImGui and ImGuizmo
include_directories("deps/imgui")
file(GLOB_RECURSE IMGUI_SOURCE "deps/imgui/*.cpp")
add_library(imgui ${IMGUI_SOURCE})
target_link_libraries(${PROJECT_NAME} imgui)

# other includes
include_directories("deps/")

# shaders
if(WIN32)
    cmake_path(SET GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
    cmake_path(SET GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
endif()
message("GLSL VALIDATOR ${GLSL_VALIDATOR}")
add_compile_definitions(GLSL_VALIDATOR="${GLSL_VALIDATOR}")
add_compile_definitions(LUZ_ENGINE)

# assets
if(UNIX)
    add_custom_target(run
        COMMAND ${CMAKE_SOURCE_DIR}/bin/${CMAKE_PROJECT_NAME}
        DEPENDS ${CMAKE_SOURCE_DIR}/bin/${CMAKE_PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} 
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

# log build types
message("-- CMake Build Type: ${CMAKE_BUILD_TYPE}")