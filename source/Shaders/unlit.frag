#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 color;
} material;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = model.colors[0]*texture(textures[model.textures[0]], fragTexCoord);
}