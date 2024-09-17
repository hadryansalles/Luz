#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform _constants {
    LineRenderingConstants ctx;
};

layout(location = 0) out vec4 outColor;

void main() {
    if (ctx.depthAware == 1) {
        vec2 uv = gl_FragCoord.xy / ctx.imageSize;
        float depth = texture(textures[ctx.depthRID], uv).r;
        if (gl_FragCoord.z >= depth) {
            discard;
        }
    }
    outColor = ctx.color;
}