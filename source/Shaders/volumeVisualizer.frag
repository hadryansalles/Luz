#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform _constants {
    VolumeVisualizerConstants ctx;
};

void main() {
    vec2 uv = gl_FragCoord.xy / ctx.imageSize;
    float depth = texture(textures[ctx.depthRID], uv).r;
    if (gl_FragCoord.z >= depth) {
        discard;
    }
    outColor = inColor;
}
