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

#define FOG_DENSITY 0.2
#define FOG_BRIGHNESS_CLAMP 0.5

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
            float fogPercent = 0.0f;
            for(int i = 0; i < steps; i++) {
                float rayPercent = float(i)/steps;
                vec3 O = scene.camPos + viewDir*rayPercent;
                vec3 D = light.position - O;
                float tMin = 0.001;
                float tMax = length(D);
                D = normalize(D);
                int hit = TraceRayHitSomething(O, D, tMin, tMax);
                fogPercent = mix(fogPercent, 1-hit, 1.0f/float(i+1));
            }
            // vec3 fogColor = mix(vec3(0,0,0), light.color*light.intensity, fogPercent);
            vec3 fogColor = mix(vec3(0,0,0), light.color, fogPercent);
            float absorb = exp(-depth*FOG_DENSITY);
            outLight.rgb = clamp(1.0f -absorb, 0.0f, FOG_BRIGHNESS_CLAMP)*fogColor;
        }
    }
}