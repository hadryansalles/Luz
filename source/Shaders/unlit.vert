#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

// some data types occupy more than one location
// like the dvec3 is 64 bit, any attribute after
// it must have an index at least 2 higher

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = scene.projView * model.modelMat * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}