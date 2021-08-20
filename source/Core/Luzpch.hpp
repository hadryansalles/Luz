#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_VULKAN
// glfw will include vulkan and its own definitions
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
// force the GLM projection matrix to use depth range of [0.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "Base.hpp"
#include "Log.hpp"
#include "Profiler.hpp"