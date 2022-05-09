#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform ShadowConstants {
    int sceneBufferIndex;
    int lightIndex;
    int frame;
    int normalRID;
    int depthRID;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out float outShadow;

#include "base.glsl"
#include "rayTracingBase.glsl"

void main() {
    outShadow = 0.0f;
    vec3 N = texture(imageAttachs[normalRID], fragTexCoord).xyz;
    float depth = texture(imageAttachs[depthRID], fragTexCoord).r;
    outShadow = depth;
    if(depth == 1.0) {
        discard;
    } else {
        vec3 fragPos = DepthToWorld(scene.inverseProj, scene.inverseView, fragTexCoord, depth);
        float shadowBias = 0.01;
        vec3 shadowOrigin = fragPos.xyz + N*shadowBias;
        LightBlock light = scene.lights[lightIndex];
        vec3 L_ = light.position - fragPos.xyz;
        vec3 L = normalize(L_);
        if(light.type == LIGHT_TYPE_DIRECTIONAL) {
            L = normalize(-light.direction);
            outShadow = TraceShadowRay(shadowOrigin, L*10000, light.numShadowSamples, light.radius);
        } else {
            float dist = length(light.position - fragPos.xyz);
            outShadow = TraceShadowRay(shadowOrigin, L*dist, light.numShadowSamples, light.radius);
        } 
    }
}