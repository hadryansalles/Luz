#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int lightIndex;
};

layout(location = 0) in vec4 fragPos;

void main() {
    LightBlock light = scene.lights[lightIndex];
    if (light.type == LIGHT_TYPE_POINT) {
        gl_FragDepth = length(light.position - fragPos.xyz) / light.zFar;
    } else {
        gl_FragDepth = gl_FragCoord.z;
    }
}