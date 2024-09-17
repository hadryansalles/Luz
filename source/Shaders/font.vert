#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform _constants {
    FontRenderingConstants ctx;
};

layout(location = 0) in vec3 inPosition;

const vec3 QUAD[6] = {
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 0.0),

    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0),
};

layout(location = 0) out vec2 fragUV;

void main() {
    vec3 fragPos = QUAD[gl_VertexIndex];
    fragUV = fragPos.xy;
    fragPos *= ctx.fontSize * 0.1;
    fragPos += vec3(ctx.charSize * 0.0004, 0, 0) * ctx.charIndex;
    vec4 fragAnchor = scene.view * vec4(ctx.pos, 1.0);
    gl_Position = scene.proj * (fragAnchor + vec4(fragPos, 1.0));
}