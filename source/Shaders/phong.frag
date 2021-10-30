#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 2, binding = 0) uniform MaterialUBO {
    vec4 color;
} material;

layout(set = 3, binding = 0) uniform sampler2D texSampler;

layout(set = 4, binding = 0) uniform PointLightUBO {
    vec4 color;
    vec3 position;
    float intensity;
} light;

layout(set = 5, binding = 0) uniform sampler2D colorTextures[];

layout(push_constant) uniform PushConstant {
    int textureID;
};

// struct PointLight {
//     vec4 color;
//     vec3 position;
//     float intensity;
// };
// 
// struct DirectionalLight {
//     vec4 color;
//     vec3 direction;
//     float intensity;
// };
// 
// layout(binding = 0) restrict readonly buffer SceneBuffer {
//     mat4 view;
//     mat4 proj;
// } sceneBuffers[];
// 
// layout(binding = 0) restrict readonly buffer LightBuffer {
//     PointLight pointLights[100];
//     DirectionalLight directionalLights[100];
//     int numPointLights;
//     int numDirectionalLights;
// } lightBuffers[];
// 
// struct PBRMaterial {
//     vec4 albedo;
//     vec4 roughness;
//     vec4 normal;
//     vec4 metal;
//     int albedoTextureID;
//     int roughnessTextureID;
//     int normalTextureID;
//     int metalTextureID;
// };
// 
// struct PhongMaterial {
//     vec4 diffuse;
//     vec4 specular;
//     int diffuseTextureID;
//     int specularTextureID;
// };
// 
// layout(binding = 0) restrict readonly buffer PhongMaterialBuffer {
//     PhongMaterial materials[4096];
// } phongBuffers[];
// 
// 
// layout(binding = 0) restrict readonly buffer PBRMaterialBuffer {
//     PBRMaterial materials[];
// } pbrBuffers[];
// 
// struct Model {
//     mat4 model;
// };
// 
// layout(binding = 0) restrict readonly buffer ModelBuffer {
//     Model models[];
// } modelBuffers[];

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

vec3 EvalDirectionalLight(vec3 color, vec3 position, float intensity) {
    vec3 lightVec = position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    return color*clamp(dot(lightVec, fragNormal), 0.0, 1.0)*intensity/dist2;
}

void main() {
    vec3 intensity = vec3(0.1, 0.1, 0.1);
    intensity += EvalPointLight(light.color.xyz, light.position, light.intensity);
    outColor = material.color*texture(colorTextures[textureID], fragTexCoord)*vec4(intensity, 1.0);
}