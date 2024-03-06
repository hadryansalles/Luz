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

layout(location = 0) in float depth;

void main() {
    gl_FragDepth = depth;
}