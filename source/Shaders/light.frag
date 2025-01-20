#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    LightConstants ctx;
};

#include "utils.glsl"
#include "atmosphericUtils.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
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

vec2 WhiteNoise(vec3 p3) {
    p3 = fract(p3 * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

vec2 DiskSample(vec2 rng, float radius) {
    float pointRadius = radius * sqrt(rng.x);
    float pointAngle = rng.y * 2.0f * PI;
    return vec2(pointRadius * cos(pointAngle), pointRadius * sin(pointAngle));
}

vec3 HemisphereSample(vec2 rng) {
    float r = sqrt(rng.x);
    float theta = 6.283 * rng.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    return vec3(x, y, sqrt(max(0.0, 1.0 - rng.x)));
}

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float TraceShadowRay(vec3 O, vec3 L, float numSamples, float radius) {
    if (numSamples == 0) {
        return 0;
    }
    vec3 lightTangent = normalize(cross(L, vec3(0, 1, 0)));
    vec3 lightBitangent = normalize(cross(lightTangent, L));
    float numShadows = 0;
    for (int i = 0; i < numSamples; i++) {
        vec2 blueNoise = BlueNoiseSample(i, gl_FragCoord.xy).rg;
        vec2 rng = blueNoise;
        vec2 diskSample = DiskSample(rng, radius);
        float tMax = length(L);
        vec3 direction = normalize(L + diskSample.x * lightTangent + diskSample.y * lightBitangent);
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, O, 0.001, direction, tMax);

        while (rayQueryProceedEXT(rayQuery)) {}

        if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            numShadows++;
        }
    }
    return numShadows / numSamples;
}

float TraceAORays(vec3 fragPos, vec3 normal) {
    if (scene.aoNumSamples == 0) {
        return 1;
    }
    float ao = 0;
    vec3 tangent = abs(normal.z) > 0.5 ? vec3(0.0, -normal.z, normal.y) : vec3(-normal.y, normal.x, 0.0);
    vec3 bitangent = cross(normal, tangent);
    float tMin = scene.aoMin;
    float tMax = scene.aoMax;
    for (int i = 0; i < scene.aoNumSamples; i++) {
        vec2 rng = BlueNoiseSample(i, gl_FragCoord.xy).rg;
        vec3 randomVec = HemisphereSample(rng);
        vec3 direction = tangent * randomVec.x + bitangent * randomVec.y + normal * randomVec.z;
        // Ray Query for shadow
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos, tMin, direction, tMax);

        while (rayQueryProceedEXT(rayQuery)) {}

        if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
            ao += 1.0;
        }
    }
    return ao / scene.aoNumSamples;
}

float SampleShadowMapTexture(LightBlock light, vec3 fragPos) {
    if (light.type == LUZ_LIGHT_TYPE_DIRECTIONAL || light.type == LUZ_LIGHT_TYPE_SUN) {
        vec4 fragInLight = (light.viewProj[0] * vec4(fragPos, 1));
        vec2 uv = (fragInLight.xy * 0.5 + vec2(0.5f, 0.5f));
        float shadowDepth = texture(textures[light.shadowMap], uv).r;
        return fragInLight.z >= shadowDepth ? 1.0f : 0.0f;
    } else {
        vec3 lightToFrag = fragPos - light.position;
        float shadowDepth = texture(cubeTextures[light.shadowMap], lightToFrag).r;
        return length(lightToFrag) >= (shadowDepth * light.zFar) ? 1.0f : 0.0f;
    }
}

float EvaluateShadowMap(LightBlock light, vec3 L, vec3 N, vec3 fragPos) {
    return SampleShadowMapPCF(light, fragPos, light.radius, gl_FragCoord.xy);
}

float EvaluateShadow(LightBlock light, vec3 L, vec3 N, vec3 fragPos) {
    float shadowBias = max(length(fragPos - scene.camPos) * 0.01, 0.05);
    vec3 shadowOrigin = fragPos.xyz + N * shadowBias;
    float dist = length(light.position - fragPos.xyz);
    if (scene.shadowType == SHADOW_TYPE_RAYTRACING) {
        if (light.type == LUZ_LIGHT_TYPE_DIRECTIONAL || light.type == LUZ_LIGHT_TYPE_SUN) {
            return TraceShadowRay(shadowOrigin, light.direction * dot(light.direction, L) * dist, light.numShadowSamples, light.radius);
        } else {
            return TraceShadowRay(shadowOrigin, L * dist, light.numShadowSamples, light.radius);
        }
    } else if (scene.shadowType == SHADOW_TYPE_MAP && light.shadowMap != -1) {
        return EvaluateShadowMap(light, L, N, fragPos);
    } else {
        return 1.0f;
    }
}

vec3 TransmittanceFromTexture(float height, float angle) {
    float u = (angle + 1.0) * 0.5;
    float v = height / (topRadius - bottomRadius);
    return texture(textures[ctx.atmosphericTransmittanceRID], vec2(u, v)).xyz;
}

vec3 MultipleScatteringContributionFromTexture(float height, float angle) {
    float u = (angle + 1.0) * 0.5;
    float v = height / (topRadius - bottomRadius);
    return texture(textures[ctx.atmosphericScatteringRID], vec2(u, v)).xyz;
}

vec3 EvaluateSky(vec3 rayDirection, LightBlock light) {
    const float sunNightIntensity = 0.001;
    float sunIntensity = light.intensity;
    rayDirection = normalize(rayDirection);
    vec3 cameraPosition = vec3(0.0, bottomRadius + 0.2e3, 0.0);
    vec3 lightDirection = normalize(-light.direction);
    int sampleCount = 32;
    vec3 lightSum = vec3(0.0);
    vec3 transmittanceFromCameraToSpace = vec3(1.0);
    float offset = 0.0;
    float distanceToSpace = 0.0;
    if (IntersectSphere(cameraPosition, rayDirection, topRadius, offset, distanceToSpace)) {
        vec3 rayOrigin = cameraPosition;
        if (offset > 0.0) { // above atmosphere
            rayOrigin += rayDirection * offset; // intersection of camera ray with atmosphere
        }
        float height = length(rayOrigin) - bottomRadius;
        float rayAngle = dot(rayOrigin, rayDirection) / length(rayOrigin);
        bool cameraBelow = rayAngle < 0.0;
        transmittanceFromCameraToSpace = TransmittanceFromTexture(height, cameraBelow ? -rayAngle : rayAngle);
        float phaseAngle = dot(lightDirection, rayDirection);
        float rayleighPhase = RayleighPhase(phaseAngle);
        float miePhase = MiePhase(phaseAngle);
        float distanceToGround = 0.0;
        bool hitGround = IntersectSphere(cameraPosition, rayDirection, bottomRadius, distanceToGround) && distanceToGround > 0.0;
        float segmentLength = ((hitGround ? distanceToGround : distanceToSpace) - max(offset, 0.0)) / float(sampleCount);
        float t = segmentLength * 0.5;
        vec3 transmittanceCamera;
        vec3 transmittanceLight;
        for (int i = 0; i < sampleCount; i++) {
            vec3 position = rayOrigin + t * rayDirection;
            float height = length(position) - bottomRadius;
            vec3 up = position / length(position);
            float rayAngle = dot(up, rayDirection);
            float lightAngle = dot(up, lightDirection);
            float shadow = 1.0;
            vec3 transmittanceToSpace = TransmittanceFromTexture(height, cameraBelow ? -rayAngle : rayAngle);
            transmittanceCamera = cameraBelow ? (transmittanceToSpace / transmittanceFromCameraToSpace) : (transmittanceFromCameraToSpace / transmittanceToSpace);

            transmittanceLight = TransmittanceFromTexture(height, lightAngle);
            vec2 opticalDensity = exp(-height / vec2(rayleighScaleHeight, mieScaleHeight));
            vec3 scatteredLight = transmittanceLight * (rayleighScatteringCoefficient * opticalDensity.x * rayleighPhase + mieScatteringCoefficient * opticalDensity.y * miePhase);
            scatteredLight += MultipleScatteringContributionFromTexture(height, lightAngle) * (rayleighScatteringCoefficient * opticalDensity.x + mieScatteringCoefficient * opticalDensity.y);

            lightSum += shadow * transmittanceCamera * scatteredLight * segmentLength;
            t += segmentLength;
        }

        lightSum *= sunIntensity;

        if (false && hitGround) {
            vec3 hitPoint = cameraPosition + rayDirection * distanceToGround;
            vec3 up = hitPoint / length(hitPoint);
            float lightAngle = dot(up, lightDirection);
            float groundAlbedo = 0.85;
            lightSum += transmittanceCamera * (groundAlbedo / pi) * MultipleScatteringContributionFromTexture(height, lightAngle) * sunIntensity;
            lightSum += transmittanceCamera * transmittanceLight * (groundAlbedo / pi) * lightAngle * sunIntensity;
        }
    }

    float distanceToGround;
    bool hitGround = IntersectSphere(cameraPosition, rayDirection, bottomRadius, distanceToGround) && distanceToGround > 0.0;
    const float sunAngularRadiusScale = 0.004685;
    const float sunDirectIntensity = 1.0;
    if (!hitGround) {
        float angle = dot(rayDirection, lightDirection);
        if (angle > cos(light.sunRadius * sunAngularRadiusScale)) {
            lightSum += sunIntensity * sunDirectIntensity * transmittanceFromCameraToSpace;
        }
    }

    vec3 color = lightSum;
    color *= 8.0;
    return color;
}

void main() {
    vec4 albedo = pow(texture(textures[ctx.albedoRID], fragTexCoord), vec4(2.2));
    vec3 N = texture(textures[ctx.normalRID], fragTexCoord).xyz;
    vec4 material = texture(textures[ctx.materialRID], fragTexCoord);
    vec4 emission = texture(textures[ctx.emissionRID], fragTexCoord);
    float depth = texture(textures[ctx.depthRID], fragTexCoord).r;

    if (length(N) == 0.0f) {
        vec3 fragPos = DepthToWorld(fragTexCoord, 1.0);
        vec3 rayDirection = normalize(fragPos - scene.camPos);
        if (scene.sunLightIndex != -1) {
            outColor = vec4(EvaluateSky(rayDirection, scene.lights[scene.sunLightIndex]), 1.0);
        } else {
            outColor = vec4(scene.ambientLightColor * scene.ambientLightIntensity, 1.0);
        }
        return;
    }

    float occlusion = material.b;
    float roughness = material.r;
    float metallic = material.g;
    vec3 fragPos = DepthToWorld(fragTexCoord, depth);
    vec3 V = normalize(scene.camPos - fragPos.xyz);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < scene.numLights; i++) {
        LightBlock light = scene.lights[i];
        vec3 L_ = light.position - fragPos.xyz;
        vec3 L = normalize(L_);
        float attenuation = 1;
        if (light.type == LUZ_LIGHT_TYPE_DIRECTIONAL || light.type == LUZ_LIGHT_TYPE_SUN) {
            L = normalize(-light.direction);
        } else if (light.type == LUZ_LIGHT_TYPE_SPOT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist * dist);
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.innerAngle - light.outerAngle;
            attenuation *= clamp((theta - light.outerAngle) / epsilon, 0.0, 1.0);
        } else if (light.type == LUZ_LIGHT_TYPE_POINT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist * dist);
        }
        float shadowFactor = EvaluateShadow(light, L, N, fragPos);
        vec3 radiance = light.color * light.intensity * attenuation * (1.0 - shadowFactor);

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
        Lo += (kD * albedo.rgb / PI + spec) * radiance * NdotL;
    }

    float shadowBias = length(fragPos - scene.camPos) * 0.01;
    vec3 shadowOrigin = fragPos.xyz + N * shadowBias;
    float rayTracedAo = TraceAORays(shadowOrigin, N);
    vec3 ambient = scene.ambientLightColor * scene.ambientLightIntensity * albedo.rgb * occlusion * rayTracedAo;
    vec3 color = ambient + Lo + emission.rgb;
    outColor = vec4(color, 1.0);
}
