#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform _constants {
    VolumeVisualizerConstants ctx;
};

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = scene.viewProj * vec4(inPosition, 1.0);
    outColor = ctx.color;
}