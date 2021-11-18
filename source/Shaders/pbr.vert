#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    fragPos = model.modelMat * vec4(inPosition, 1.0);
    gl_Position = scene.projView * fragPos;
    fragNormal = normalize(vec3(transpose(inverse(model.modelMat))*vec4(inNormal, 1.0)));
    fragTexCoord = inTexCoord;
}