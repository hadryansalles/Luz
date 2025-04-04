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

void main() {
    vec2 fragTexCoord = gl_FragCoord.xy / vec2(textureSize(textures[ctx.lightRID], 0));
    float depth = texture(textures[ctx.depthRID], fragTexCoord).r;
    if (gl_FragCoord.z >= depth) {
        discard;
    }
    vec4 rgba = texture(textures[ctx.lightRID], fragTexCoord);
    outColor = rgba + vec4(0.1, 0.1, 0.1, 1.0);
    return;

//     // Get the light and scene data
//     LightBlock light = scene.lights[ctx.lightIndex];
    
//     // Sample depth texture
    
//     // Reconstruct world position from depth
//     vec2 texCoord = gl_FragCoord.xy / vec2(textureSize(textures[ctx.depthRID], 0));
//     vec4 clipPos = vec4(texCoord * 2.0 - 1.0, sceneDepth, 1.0);
//     vec4 viewPos = scene.inverseProj * clipPos;
//     viewPos /= viewPos.w;
//     vec3 worldPosFromDepth = (scene.inverseView * vec4(viewPos.xyz, 1.0)).xyz;
    
//     // Calculate view ray direction
//     vec3 viewDir = normalize(inWorldPos - scene.camPos);
    
//     // Phase function (simplified Henyey-Greenstein)
//     float g = 0.2; // Asymmetry parameter
//     float cosTheta = dot(viewDir, normalize(-inLightDir));
//     float phase = (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0*g*cosTheta, 1.5));
    
//     // Calculate distance to scene geometry
//     float rayLength = distance(scene.camPos, worldPosFromDepth);
//     float volumeDepth = distance(scene.camPos, inWorldPos);
//     float stepLength = min(rayLength, volumeDepth);
    
//     // Apply scattering
//     float transmittance = exp(-light.volumetricAbsorption * stepLength);
//     float scattering = light.scatteringCoefficient * phase * (1.0 - transmittance) / light.volumetricAbsorption;
    
//     // Apply light color and intensity
//     vec3 volumeLight = light.color * light.intensity * scattering;
    
//     // Check if we're inside the volume
//     if (inDepth > sceneDepth) {
//         outColor = vec4(volumeLight, 1.0);
//     } else {
//         discard; // Outside the volume or behind scene geometry
//     }
}