#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    ShadowMapConstants ctx;
};

layout(location = 0) in vec4 fragPos;

void main() {
    LightBlock light = scene.lights[ctx.lightIndex];
    if (light.type == LIGHT_TYPE_DIRECTIONAL) {
        gl_FragDepth = gl_FragCoord.z;
    } else {
        gl_FragDepth = length(light.position - fragPos.xyz) / light.zFar;
    }
}