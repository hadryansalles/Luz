#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int frame;
};

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

vec2 WhiteNoise(vec3 p3) {
	p3 = fract(p3 * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx+33.33);
    return fract((p3.xx+p3.yz)*p3.zy);
}

vec2 DiskSample(vec2 rng, float radius) {
    float pointRadius = radius*sqrt(rng.x);
    float pointAngle = rng.y * 2.0f * PI;
    return vec2(pointRadius*cos(pointAngle), pointRadius*sin(pointAngle));
}

vec3 HemisphereSample(vec2 rng) {
    float r = sqrt(rng.x);
    float theta = 6.283 * rng.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    return vec3(x, y, sqrt(max(0.0, 1.0 - rng.x)));
}

vec4 BlueNoiseSample(int i) {
    vec2 blueNoiseSize = textureSize(BLUE_NOISE_TEXTURE, 0);
    ivec2 fragUV = ivec2(mod(gl_FragCoord.xy + GOLDEN_RATIO*blueNoiseSize*(frame%64 + i*vec2(5, 7)), blueNoiseSize));
    return texelFetch(BLUE_NOISE_TEXTURE, fragUV, 0);
}

float TraceAORays(vec3 normal) {
    if(scene.aoNumSamples == 0) {
        return 1;
    }
    float ao = 0;
    vec3 tangent = abs(normal.z) > 0.5 ? vec3(0.0, -normal.z, normal.y) : vec3(-normal.y, normal.x, 0.0);
    vec3 bitangent = cross(normal, tangent);
    float tMin = scene.aoMin;
    float tMax = scene.aoMax;
    for(int i = 0; i < scene.aoNumSamples; i++) {
        // vec2 whiteNoise = WhiteNoise(vec3(gl_FragCoord.xy, float(frame * scene.aoNumSamples + i)));
        vec2 whiteNoise = WhiteNoise(vec3(gl_FragCoord.xy, float(frame + i)));
        vec2 blueNoise = BlueNoiseSample(scene.aoNumSamples + i).rg;
        // vec2 blueNoise = texture(BLUE_NOISE_TEXTURE, fragCoord).xy;
        vec2 rng = (scene.useBlueNoise) * blueNoise + (1 - scene.useBlueNoise)*whiteNoise;
        // vec2 rng = WhiteNoise(vec3(WhiteNoise(fragPos.xyz*(frame%128 + 1)), frame%16 + scene.aoNumSamples*i));
        vec3 randomVec = HemisphereSample(rng);
        vec3 direction = tangent*randomVec.x + bitangent*randomVec.y + normal*randomVec.z;
        // Ray Query for shadow
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos.xyz, tMin, direction, tMax);

        while(rayQueryProceedEXT(rayQuery)) {}

        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            ao += rayQueryGetIntersectionTEXT(rayQuery, true)/(tMax*scene.aoNumSamples);
        } else {
            ao += 1.0/float(scene.aoNumSamples);
            // ao += 1;
        }
    }
    return clamp(pow(ao, scene.aoPower), 0.0, 1.0);
}

vec3 TraceReflectionRay(vec3 N, vec3 V) {
    // V = from cam to frag
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(fragPos.xyz - scene.camPos);
    vec3 direction = reflect(viewDir, normal);
    float tMin = 0.1;
    float tMax = 10000.0;
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos.xyz, tMin, direction, tMax);

    while(rayQueryProceedEXT(rayQuery)) {}

    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        int id = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
        return modelsBuffers[modelBufferIndex].models[id].color.rgb;
    } else {
        return vec3(0, 0, 0);
    }
}

float TraceShadowRay(vec3 O, vec3 L, float numSamples, float radius) {
    if(numSamples == 0) {
        return 0;
    }
    vec3 lightTangent = normalize(cross(L, vec3(0, 1, 0)));
    vec3 lightBitangent = normalize(cross(lightTangent, L));
    float numShadows = 0;
    for(int i = 0; i < numSamples; i++) {
        vec2 whiteNoise = WhiteNoise(vec3(gl_FragCoord.xy, float(frame * numSamples + i)));
        vec2 blueNoise = BlueNoiseSample(scene.aoNumSamples + i).rg;
        vec2 rng = (scene.useBlueNoise) * blueNoise + (1 - scene.useBlueNoise)*whiteNoise;
        // vec2 rng = WhiteNoise(vec3(WhiteNoise(fragPos.xyz*(frame%128 + 1)), frame%16 + numSamples*i));
        vec2 diskSample = DiskSample(rng, radius);
        // vec2 diskSample = BlueNoiseInDisk[(i+frame)%64];
        // Ray Query for shadow
        float tMax = length(L);
        vec3 direction = normalize(L + diskSample.x * lightTangent + diskSample.y * lightBitangent);
        // Initializes a ray query object but does not start traversal
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, O, 0.001, direction, tMax);

        // Start traversal: return false if traversal is complete
        while(rayQueryProceedEXT(rayQuery)) {}

        // Returns type of committed (true) intersection
        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            // Got an intersection == Shadow
            numShadows++;
        }
    }
    return numShadows/numSamples;
}

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
    vec3 V = normalize(scene.camPos - fragPos.xyz);
    // if(metallic == 1) {
    //     outColor = vec4(TraceReflectionRay(N, V), 1.0);
    // }
    // else {
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < scene.numLights; i++) {
        LightBlock light = scene.lights[i];
        vec3 L = normalize(light.position - fragPos.xyz);
        float attenuation = 1;
        float shadowFactor = 1.0;
        if(light.type == LIGHT_TYPE_DIRECTIONAL) {
            L = normalize(-light.direction);
            shadowFactor = TraceShadowRay(fragPos.xyz, L*10000, light.numShadowSamples, light.radius);
        } else if(light.type == LIGHT_TYPE_SPOT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist*dist);
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.innerAngle - light.outerAngle;
            attenuation *= clamp((theta - light.outerAngle)/epsilon, 0.0, 1.0);
            shadowFactor = TraceShadowRay(fragPos.xyz, L*dist, light.numShadowSamples, light.radius);
        } else if(light.type == LIGHT_TYPE_POINT) {
            float dist = length(light.position - fragPos.xyz);
            attenuation = 1.0 / (dist*dist);
            shadowFactor = TraceShadowRay(fragPos.xyz, L*dist, light.numShadowSamples, light.radius);
        }
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
        Lo += (kD * albedo.rgb / PI + spec)*radiance*NdotL;
    }

    float rayTracedAo = TraceAORays(N);
    vec3 ambient = scene.ambientLightColor*scene.ambientLightIntensity*albedo.rgb*occlusion*rayTracedAo;

    vec3 color = ambient + Lo + emission.rgb;
    color = color / (color + vec3(1.0));
    outColor = vec4(pow(color, vec3(1.0/2.2)), albedo.a);
    // }
}