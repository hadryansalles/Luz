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
    if (light.type == 0) {
        gl_FragDepth = 100*length(fragPos.xyz - light.position) / light.zFar;
        // gl_FragDepth = 0;
    } else {
        gl_FragDepth = fragPos.z / light.zFar;
    }
}