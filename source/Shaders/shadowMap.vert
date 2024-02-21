#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    mat4 lightViewProj;
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int pad;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out float depth;

void main() {
    vec4 fragPos = model.modelMat * vec4(inPosition, 1.0);
    gl_Position = lightViewProj * fragPos;
    depth = gl_Position.z;
}