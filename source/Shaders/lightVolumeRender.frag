#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    LightVolumeRenderConstants ctx;
};

layout(location = 0) out vec4 outColor;

// Phase function: Henyey-Greenstein
float phaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

void main() {
    vec2 fragTexCoord = gl_FragCoord.xy / vec2(textureSize(textures[ctx.lightRID], 0));
    float sceneDepth = texture(textures[ctx.depthRID], fragTexCoord).r;
    const float depth = min(gl_FragCoord.z, sceneDepth);

    float entering = gl_FrontFacing ? 1.0 : -1.0;
    LightBlock light = scene.lights[ctx.lightIndex];
    
    // Calculate view direction
    vec3 viewPos = scene.camPos;
    
    // Calculate ray direction from fragment to camera
    vec4 clipPos = vec4(fragTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos4 = scene.inverseProj * clipPos;
    vec3 viewDir = normalize((scene.inverseView * vec4(viewPos4.xyz / viewPos4.w, 0.0)).xyz);
    
    // For directional light
    vec3 lightDir = normalize(-light.direction);
    
    // Calculate the cosine of the angle between view direction and light direction
    float cosTheta = dot(viewDir, lightDir);
    
    // Parameters for light scattering
    float extinction = light.volumetricAbsorption + light.volumetricScattering;
    float scattering = light.volumetricScattering;
    
    // Calculate phase function (scattering distribution)
    // Using Henyey-Greenstein phase function with light's scattering coefficient
    float g = light.scatteringCoefficient; // Asymmetry parameter [-1,1]
    float phase = phaseHG(cosTheta, g);
    
    // Calculate light intensity at this depth
    // Convert from normalized depth to world distance
    float distanceTraveled = (2.0 * scene.proj[3][2]) / (depth * 2.0 - 1.0 - scene.proj[2][2]);
    
    // The equation: L(d, l)*pm(xl, wx)*(1-exp(-Tex * d)) / Tex
    // where:
    // L(d, l) = light intensity at depth d in direction l
    // pm(xl, wx) = phase function
    // Tex = extinction coefficient
    // d = distance traveled
    
    // Light intensity (radiance)
    vec3 lightIntensity = light.color * light.intensity;
    
    // Implement the scattering equation
    float transmittance = exp(-extinction * distanceTraveled);
    float scatteringIntegral = (1.0 - transmittance) / extinction;
    
    // Final value is light intensity * phase function * scattering integral * scattering coefficient
    vec3 scatteredLight = lightIntensity * phase * scatteringIntegral * scattering;
    
    outColor = vec4(scatteredLight * 100000 * entering, 1.0);
}