#version 460

#extension GL_GOOGLE_include_directive : enable

layout(early_fragment_tests) in;

layout(push_constant) uniform ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

#include "base.glsl"

layout(location = 0) in vec3 fragPos;

layout(location = 0) out vec4 outAlbedo;

void main() {
    outAlbedo = texture(cubeTextures[scene.envmap], fragPos);
}