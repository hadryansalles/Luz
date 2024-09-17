#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform _constants {
    LineRenderingConstants ctx;
};

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = scene.projView * vec4(inPosition, 1.0);
}