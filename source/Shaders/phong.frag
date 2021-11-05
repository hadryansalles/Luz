#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 color;
} material;

struct PointLight {
    vec4 color;
    vec3 position;
    float intensity;
};

struct DirectionalLight {
    vec4 color;
    vec3 direction;
    float intensity;
};

struct SpotLight {
    vec3 color;
    float intensity;
    vec3 direction;
    float innerAngle;
    vec3 position;
    float outerAngle;
};

layout(set = 2, binding = 1) uniform LightsBuffer {
    PointLight pointLights[10];
    DirectionalLight directionalLights[10];
    SpotLight spotLights[10];
    vec3 ambientColor;
    float ambientIntensity;
    int numPointLights;
    int numDirectionalLights;
    int numSpotLights;
} lightsBuffers[];

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

vec3 EvalPointLight(PointLight pointLight) {
    vec3 lightVec = pointLight.position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    return pointLight.color.xyz*clamp(dot(lightVec, fragNormal), 0.0, 1.0)*pointLight.intensity/dist2;
}

vec3 EvalDirectionalLight(DirectionalLight dirLight) {
    return dirLight.color.xyz*clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0)*dirLight.intensity;
}

vec3 EvalSpotLight(SpotLight light) {
    vec3 lightVec = light.position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    float theta = dot(lightVec, normalize(-light.direction));
    float epsilon = light.innerAngle - light.outerAngle;
    float diff = clamp(dot(fragNormal, -light.direction), 0.0, 1.0);
    return light.color*diff*clamp((theta-light.outerAngle)/epsilon, 0.0, 1.0)*light.intensity/dist2;
}

void main() {
    // outColor = vec4(1, 1, 1, 1);
    vec3 intensity = scene.ambientLightColor*scene.ambientLightIntensity;
    // for(int i = 0; i < lights.numPointLights; i++) {
    //     intensity += EvalPointLight(lights.pointLights[i]);
    // }
    // for(int i = 0; i < lights.numDirectionalLights; i++) {
    //     intensity += EvalDirectionalLight(lights.directionalLights[i]);
    // }
    // for(int i = 0; i < lights.numSpotLights; i++) {
    //     intensity += EvalSpotLight(lights.spotLights[i]);
    // }
    // outColor = model.colors[0]*texture(textures[model.textures[0]], fragTexCoord)*vec4(intensity, 1.0);
    outColor = model.colors[0]*texture(textures[model.textures[0]], fragTexCoord)*vec4(intensity, 1.0);
}