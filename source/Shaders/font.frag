#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform _constants {
    FontRenderingConstants ctx;
};

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    if (ctx.depthAware == 1) {
        vec2 uv = gl_FragCoord.xy / textureSize(textures[ctx.depthRID], 0).xy;
        float depth = texture(textures[ctx.depthRID], uv).r;
        if (gl_FragCoord.z >= depth) {
            discard;
        }
    }
    vec2 charOffset = ivec2(ctx.charCode % 16, ctx.charCode / 16) * ctx.charSize;
    vec2 fragOffset = (vec2(fragUV.x, 1.0 - fragUV.y) + 0.1) * ctx.charSize;
    vec2 uv = (charOffset + fragOffset) / (ctx.charSize * 16);
    float bit = texture(textures[ctx.fontRID], uv).r;
    if (bit > 0) {
        discard;
    }
    outColor = ctx.color;
}