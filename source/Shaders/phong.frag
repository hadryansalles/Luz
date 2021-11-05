#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

vec3 EvalPointLight(LightBlock light) {
    vec3 lightVec = light.position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    return light.color.xyz*clamp(dot(lightVec, fragNormal), 0.0, 1.0)*light.intensity/dist2;
}
// 
// vec3 EvalDirectionalLight(DirectionalLight dirLight) {
//     return dirLight.color.xyz*clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0)*dirLight.intensity;
// }
// 
// vec3 EvalSpotLight(SpotLight light) {
//     vec3 lightVec = light.position - vec3(fragPos);
//     float dist2 = dot(lightVec, lightVec);
//     lightVec = normalize(lightVec);
//     float theta = dot(lightVec, normalize(-light.direction));
//     float epsilon = light.innerAngle - light.outerAngle;
//     float diff = clamp(dot(fragNormal, -light.direction), 0.0, 1.0);
//     return light.color*diff*clamp((theta-light.outerAngle)/epsilon, 0.0, 1.0)*light.intensity/dist2;
// }

void main() {
    // outColor = vec4(1, 1, 1, 1);
    vec3 intensity = scene.ambientLightColor*scene.ambientLightIntensity;
    for(int i = 0; i < scene.numPointLights; i++) {
        intensity += EvalPointLight(scene.pointLights[i]);
    }
    // for(int i = 0; i < lights.numDirectionalLights; i++) {
    //     intensity += EvalDirectionalLight(lights.directionalLights[i]);
    // }
    // for(int i = 0; i < lights.numSpotLights; i++) {
    //     intensity += EvalSpotLight(lights.spotLights[i]);
    // }
    // outColor = model.colors[0]*texture(textures[model.textures[0]], fragTexCoord)*vec4(intensity, 1.0);
    outColor = model.colors[0]*texture(textures[model.textures[0]], fragTexCoord)*vec4(intensity, 1.0);
}