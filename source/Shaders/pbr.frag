#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// vec4 EvalPointLight(LightBlock light) {
//     vec3 lightVec = light.position - vec3(fragPos);
//     float dist2 = dot(lightVec, lightVec);
//     lightVec = normalize(lightVec);
//     float diffuse = clamp(dot(lightVec, fragNormal), 0.0, 1.0)/dist2;
//     
//     vec3 reflectDir = reflect(-lightVec, fragNormal);
//     float specular = pow(max(dot(viewDir, reflectDir), 0.0), shineness);
//     
//     return (diffuse*diffuseColor + specular*specularColor)*vec4(light.color*light.intensity, 1.0);
// }
// 
// vec4 EvalDirectionalLight(LightBlock dirLight) {
//     float diffuse = clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0);
//     return vec4(0, 0, 0, 1);
//     // return dirLight.color.xyz*clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0)*dirLight.intensity;
// }
// 
// vec4 EvalSpotLight(LightBlock light) {
//     vec3 lightVec = light.position - vec3(fragPos);
//     float dist2 = dot(lightVec, lightVec);
//     lightVec = normalize(lightVec);
//     float theta = dot(lightVec, normalize(-light.direction));
//     float epsilon = light.innerAngle - light.outerAngle;
//     float diff = clamp(dot(fragNormal, -light.direction), 0.0, 1.0);
//     return vec4(0, 0, 0, 1);
//     // return light.color*diff*clamp((theta-light.outerAngle)/epsilon, 0.0, 1.0)*light.intensity/dist2;
// }

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 rgbaAlbedo = texture(textures[model.colorMap], fragTexCoord)*vec4(model.color, 1);
    vec3 albedo = rgbaAlbedo.rgb;
    float roughness = (texture(textures[model.roughnessMap], fragTexCoord)).r*model.roughness;
    float metallic = (texture(textures[model.metallicMap], fragTexCoord)).r*model.metallic;
    float specular = model.specular;
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.camPos - fragPos.xyz);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < scene.numLights; i++) {
        LightBlock light = scene.lights[i];
        if(light.type == LIGHT_TYPE_POINT || light.type == LIGHT_TYPE_SPOT) {
            vec3 L = normalize(light.position - fragPos.xyz);
            vec3 H = normalize(V + L);
            float dist = length(light.position - fragPos.xyz);
            float attenuation = 1.0 / (dist*dist);
            vec3 radiance = light.color * light.intensity * attenuation;

            float NDF = DistributionGGX(N, H, model.roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

            vec3 num = NDF * G * F;
            float denom = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
            vec3 spec = num / denom;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            float NdotL = max(dot(N, L), 0.0);
            Lo += (kD * albedo / PI + spec)*radiance*NdotL;
        }
    }

    Lo += albedo*model.emission;
    float alpha = rgbaAlbedo.a * model.opacity;
    vec3 ambient = scene.ambientLightColor*scene.ambientLightIntensity*albedo;
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    outColor = vec4(pow(color, vec3(1.0/2.2)), alpha);
}