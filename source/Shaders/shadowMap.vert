#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform ConstantsBlock {
    mat4 lightProj;
    mat4 lightView;
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int numSamples;
};

#include "base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out float depth;

void main() {
    vec4 fragPos = model.modelMat * vec4(inPosition, 1.0);
    // vec4 fragPos = vec4(inPosition, 1.0);
    // gl_Position = transpose(lightProj) * transpose(lightView) * fragPos;
    // gl_Position = lightProj * lightView * fragPos;
    gl_Position = lightProj * fragPos;
    depth = gl_Position.z;
    // gl_Position = fragPos;
}