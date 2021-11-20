#version 460

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragTangent;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

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

float rand(float n){return fract(sin(n) * 43758.5453123);}

vec3 cosineSampleHemisphere(vec2 rand) {
    float r = sqrt(rand.x);
    float theta = 6.283 * rand.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    return vec3(x, y, sqrt(max(0.0, 1.0 - rand.x)));
}

void main() {
    vec4 albedo = texture(textures[model.colorMap], fragTexCoord)*model.color;
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
    vec3 V = normalize(scene.camPos - fragPos.xyz);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < scene.numLights; i++) {
        LightBlock light = scene.lights[i];
        vec3 L = normalize(light.position - fragPos.xyz);
        float attenuation = 1;
        if(light.type == LIGHT_TYPE_DIRECTIONAL) {
            L = normalize(-light.direction);
        } else if(light.type == LIGHT_TYPE_SPOT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist*dist);
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.innerAngle - light.outerAngle;
            attenuation *= clamp((theta - light.outerAngle)/epsilon, 0.0, 1.0);

        } else if(light.type == LIGHT_TYPE_POINT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist*dist);
        }
        vec3 radiance = light.color * light.intensity * attenuation;

        vec3 H = normalize(V + L);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 num = NDF * G * F;
        float denom = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 spec = num / denom;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo.rgb / PI + spec)*radiance*NdotL;
    }

    vec3 ambient = scene.ambientLightColor*scene.ambientLightIntensity*albedo.rgb*occlusion;

    float numShadows = 0;
    float maxShadows = 8;
    for(int i = 0; i < maxShadows; i++) {
        // Ray Query for shadow
        vec3  origin    = fragPos.xyz;
        vec3 lightPos = scene.lights[0].position;
        lightPos += 0.6*vec3(rand(i-1), rand(i), rand(i+1));
        vec3  direction = normalize(lightPos - fragPos.xyz);  // vector to light
        float tMin      = 0.01f;
        float tMax      = length(lightPos - fragPos.xyz);

        // Initializes a ray query object but does not start traversal
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tMin, direction, tMax);

        // Start traversal: return false if traversal is complete
        while(rayQueryProceedEXT(rayQuery)) {}

        // Returns type of committed (true) intersection
        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            // Got an intersection == Shadow
            numShadows++;
        }
    }

    Lo *= 1.0 - numShadows/maxShadows;

    vec3 color = ambient + Lo + emission.rgb;
    color = color / (color + vec3(1.0));
    outColor = vec4(pow(color, vec3(1.0/2.2)), albedo.a);
}