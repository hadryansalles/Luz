#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform LightConstants {
    int sceneBufferIndex;
    int frame;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
    int envmapRID;
    int volumetricLightRID;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outLight;

#include "base.glsl"
#include "rayTracingBase.glsl"

void main() {
    float depth = texture(imageAttachs[depthRID], fragTexCoord).r;
    vec3 worldSpacePos = DepthToWorld(scene.inverseProj, scene.inverseView, fragTexCoord, depth);
    vec3 viewDir = worldSpacePos - scene.camPos;
    // vec3 viewDirN = normalize(worldSpacePos - scene.camPos);
    outLight = vec4(0, 0, 0, 1);
    for(int lightIndex = 0; lightIndex < scene.numLights; lightIndex++) {
        LightBlock light = scene.lights[lightIndex];
        float steps = 32;
        float hits = 0;
        if(light.type == LIGHT_TYPE_DIRECTIONAL) {
            vec3 D = normalize(-light.direction);
            for(int i = 0; i < steps; i++) {
                vec3 O = scene.camPos + viewDir*i/steps;
                float tMin = 0.001;
                float tMax = 1000.0;
                hits += TraceRayHitSomething(O, D, tMin, tMax);
            }
            outLight.rgb += light.color*light.intensity*(1.0-hits/steps);

        } else {
            for(int i = 0; i < steps; i++) {
                vec3 O = scene.camPos + viewDir*i/steps;
                vec3 D = light.position - O;
                float tMin = 0.001;
                float tMax = length(D);
                D = normalize(D);
                hits += (1.0 - TraceRayHitSomething(O, D, tMin, tMax))/(steps*tMax);
            }
            outLight.rgb += light.color*light.intensity*hits;
        }
    }
}