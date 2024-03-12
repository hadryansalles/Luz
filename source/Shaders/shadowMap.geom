#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int lightIndex;
};

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

layout(location = 0) out vec4 fragPos;

void main() {
    LightBlock light = scene.lights[lightIndex];
    if  (light.type == 0) {
        for(int face = 0; face < 6; face++) {
            gl_Layer = face;
            for(int i = 0; i < 3; i++) {
                fragPos = gl_in[i].gl_Position;
                gl_Position = light.viewProj[face] * fragPos;
                EmitVertex();
            }
            EndPrimitive();
        }
    } else {
        gl_Layer = 0;
        for(int i = 0; i < 3; i++) {
            fragPos = gl_in[i].gl_Position;
            gl_Position = light.viewProj[0] * fragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
} 