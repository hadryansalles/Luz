#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    LightVolumeRenderConstants ctx;
};

layout(location = 0) out vec4 outColor;

// Henyey-Greenstein phase function - versatile for various media
float phaseHenyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

void main() {
    LightBlock light = scene.lights[ctx.lightIndex];
    vec2 fragTexCoord = gl_FragCoord.xy / vec2(textureSize(textures[ctx.lightRID], 0));
    float sceneDepth = texture(textures[ctx.depthRID], fragTexCoord).r;
    float depth = min(gl_FragCoord.z, sceneDepth);
    float entering = gl_FrontFacing ? 1.0 : -1.0;
    
    vec3 viewPos = scene.camPos;
    vec4 clipPos = vec4(fragTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos4 = scene.inverseProj * clipPos;
    vec3 fragPos = (scene.inverseView * vec4(viewPos4.xyz / viewPos4.w, 0.0)).xyz;
    vec3 viewDir = normalize(fragPos - viewPos);
    vec3 lightDir = normalize(-light.direction);
    float distanceTraveled = length(fragPos - viewPos);

    float cosTheta = dot(viewDir, lightDir);
    float phase = phaseHenyeyGreenstein(cosTheta, light.volumetricAnisotropy);
    
    vec3 lightIntensity = light.color * light.intensity;
    
    float transmittance = exp(-light.volumetricExtinction * distanceTraveled);

    float scatteringIntegral = (1.0 - transmittance) / max(light.volumetricExtinction, 0.001);
    
    vec3 scatteredLight = lightIntensity * phase * scatteringIntegral;
    
    outColor = vec4(scatteredLight * entering, 1.0);
}