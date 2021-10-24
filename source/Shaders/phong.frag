#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

layout(set = 2, binding = 0) uniform MaterialUBO {
    vec4 color;
} material;

layout(set = 3, binding = 0) uniform sampler2D texSampler;

layout(set = 4, binding = 0) uniform PointLightUBO {
    vec4 color;
    vec3 position;
    float intensity;
} light;

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

vec3 EvalPointLight(vec3 color, vec3 position, float intensity) {
    vec3 lightVec = position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    return color*clamp(dot(lightVec, fragNormal), 0.0, 1.0)*intensity/dist2;
}

void main() {
    vec3 intensity = vec3(0.1, 0.1, 0.1);
    intensity += EvalPointLight(light.color.xyz, light.position, light.intensity);
    outColor = material.color*texture(texSampler, fragTexCoord)*vec4(intensity, 1.0);
}