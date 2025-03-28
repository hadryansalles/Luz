#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    LightVolumeRenderConstants ctx;
};

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outLightDir;
layout(location = 2) out float outDepth;

void main() {
    LightBlock light = scene.lights[ctx.lightIndex];
    
    vec4 worldPos = vec4(inPosition, 1.0);
    gl_Position = scene.viewProj * worldPos;
    
    outWorldPos = worldPos.xyz;
    outLightDir = light.direction;
    outDepth = gl_Position.z / gl_Position.w;
}