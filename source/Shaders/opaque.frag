#version 460

#extension GL_GOOGLE_include_directive : enable

layout(early_fragment_tests) in;

layout(push_constant) uniform ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
};

#include "base.glsl"

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragTangent;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in mat3 fragTBN;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec4 outEmission;

void main() {
    vec4 albedo = texture(textures[model.colorMap], fragTexCoord)*model.color;
    if(albedo.a < 0.5) {
        discard;
    }
    vec4 metallicRoughness = texture(textures[model.metallicRoughnessMap], fragTexCoord);
    vec4 emission = texture(textures[model.emissionMap], fragTexCoord)*vec4(model.emission, 1.0);
    vec3 normalSample = texture(textures[model.normalMap], fragTexCoord).rgb;
    float occlusion = texture(textures[model.aoMap], fragTexCoord).r;
    float roughness = metallicRoughness.g*model.roughness;
    float metallic = metallicRoughness.b*model.metallic;
    vec3 N;
    if(fragTangent == vec3(0, 0, 0) || normalSample == vec3(1, 1, 1)) {
        N = normalize(fragNormal); 
    } else {
        N = normalize(fragTBN*normalize(normalSample*2.0 - 1.0));
    }
    outAlbedo = albedo; 
    outNormal = vec4(N, 1.0);
    outMaterial = vec4(roughness, metallic, occlusion, 1.0);
    outEmission = emission;
}