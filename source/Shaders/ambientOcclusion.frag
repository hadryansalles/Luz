#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform PresentConstants {
    int sceneBufferIndex;
    int frame;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
    int volumetricLightRID;
    int ambientOcclusionRID;
};

#include "base.glsl"
#include "rayTracingBase.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out float outValue;

void main() {
    vec3 N = texture(imageAttachs[normalRID], fragTexCoord).xyz;
    float depth = texture(imageAttachs[depthRID], fragTexCoord).r;
    if(depth == 1.0) {
        discard;
    } else {
        vec3 fragPos = DepthToWorld(scene.inverseProj, scene.inverseView, fragTexCoord, depth);
        float shadowBias = 0.01;
        vec3 shadowOrigin = fragPos.xyz + N*shadowBias;
        float rayTracedAo = TraceAORays(shadowOrigin, N);
        outValue = rayTracedAo;
    }
}