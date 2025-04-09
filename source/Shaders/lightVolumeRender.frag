#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    LightVolumeRenderConstants ctx;
};

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inLightDir;
layout(location = 2) in float inDepth;

layout(location = 0) out vec4 outColor;

float henyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float schlickPhase(float cosTheta, float g) {
    float k = 1.55 * g - 0.55 * g * g * g;
    float kSq = k * k;
    return (1.0 - kSq) / (4.0 * PI * pow(1.0 + k * cosTheta, 2.0));
}

void main() {
    vec2 fragTexCoord = gl_FragCoord.xy / vec2(textureSize(textures[ctx.lightRID], 0));
    float sceneDepth = texture(textures[ctx.depthRID], fragTexCoord).r;
    
    if (gl_FragCoord.z > sceneDepth) {
        discard;
    }
    
    LightBlock light = scene.lights[ctx.lightIndex];
    
    float scatteringCoeff = light.volumetricScattering;
    float absorptionCoeff = light.volumetricAbsorption;
    float extinctionCoeff = scatteringCoeff + absorptionCoeff;
    
    float densityNoise = mix(0.9, 1.1, noise(inWorldPos * 0.5));
    scatteringCoeff *= densityNoise;
    absorptionCoeff *= densityNoise;
    extinctionCoeff = scatteringCoeff + absorptionCoeff;
    
    float g = light.scatteringCoefficient;
    
    vec3 viewDir = normalize(scene.camPos - inWorldPos);
    float cosTheta = dot(viewDir, normalize(inLightDir));
    
    float phase = henyeyGreenstein(cosTheta, g);
    
    float distanceToCamera = length(scene.camPos - inWorldPos);
    float transmittance = exp(-extinctionCoeff * distanceToCamera);
    
    vec3 inScattering = light.color * light.intensity * scatteringCoeff * phase;
    
    float multipleScatteringFactor = 1.0 + 0.5 * g * g;
    inScattering *= multipleScatteringFactor;
    
    vec3 volumetricLight;
    if (extinctionCoeff > 0.0001) {
        volumetricLight = inScattering * (1.0 - transmittance) / extinctionCoeff;
    } else {
        volumetricLight = inScattering * distanceToCamera;
    }
    
    if (light.type == LUZ_LIGHT_TYPE_POINT || light.type == LUZ_LIGHT_TYPE_SPOT) {
        float distToLight = length(light.position - inWorldPos);
        float falloff = 1.0 / max(1.0, distToLight * distToLight);
        volumetricLight *= falloff;
    }
    
    float alpha = 1.0 - transmittance;
    alpha *= light.volumetricWeight;
    outColor = vec4(volumetricLight, alpha);
}