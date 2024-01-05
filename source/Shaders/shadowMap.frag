#version 460

// layout(early_fragmentrtests) in;

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform ConstantsBlock {
    mat4 lightProj;
    mat4 lightView;
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int numSamples;
};

#include "base.glsl"

layout(location = 0) in float depth;

void main() {
    gl_FragDepth = depth*50;
    // gl_FragDepth = 413;
    // gl_FragDepth = numSamples/128.0;
    // gl_FragDepth = 0;
}