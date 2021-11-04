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

layout(set = 5, binding = 1) uniform LightsBuffer {
    PointLight pointLights[10];
    DirectionalLight directionalLights[10];
    SpotLight spotLights[10];
    vec3 ambientColor;
    float ambientIntensity;
    int numPointLights;
    int numDirectionalLights;
    int numSpotLights;
} lightsBuffers[];

layout(push_constant) uniform PushConstant {
    int frameID;
    int numFrames;
    int textureID;
};

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

#define LIGHT_BUFFER_INDEX 0
#define lights lightsBuffers[numFrames*LIGHT_BUFFER_INDEX + frameID]

void main() {
    vec3 intensity = lights.ambientColor*lights.ambientIntensity;
    for(int i = 0; i < lights.numPointLights; i++) {
        intensity += EvalPointLight(lights.pointLights[i]);
    }
    for(int i = 0; i < lights.numDirectionalLights; i++) {
        intensity += EvalDirectionalLight(lights.directionalLights[i]);
    }
    for(int i = 0; i < lights.numSpotLights; i++) {
        intensity += EvalSpotLight(lights.spotLights[i]);
    }
    outColor = material.color*texture(colorTextures[textureID], fragTexCoord)*vec4(intensity, 1.0);
}