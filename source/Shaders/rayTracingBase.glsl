#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

vec4 BlueNoiseSample(int i) {
    vec2 blueNoiseSize = textureSize(BLUE_NOISE_TEXTURE, 0);
    ivec2 fragUV = ivec2(mod(gl_FragCoord.xy + GOLDEN_RATIO*blueNoiseSize*(frame%64 + i*vec2(5, 7)), blueNoiseSize));
    return texelFetch(BLUE_NOISE_TEXTURE, fragUV, 0);
}

vec2 DiskSample(vec2 rng, float radius) {
    float pointRadius = radius*sqrt(rng.x);
    float pointAngle = rng.y * 2.0f * PI;
    return vec2(pointRadius*cos(pointAngle), pointRadius*sin(pointAngle));
}

int TraceRayHitSomething(vec3 O, vec3 D, float tMin, float tMax) {
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, O, tMin, D, tMax);

    // Start traversal: return false if traversal is complete
    while(rayQueryProceedEXT(rayQuery)) {}

    // Returns type of committed (true) intersection
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // Got an intersection == Shadow
        return 1;
    }
    return 0;
}

float TraceShadowRay(vec3 O, vec3 L, int numSamples, float radius) {

    if(numSamples == 0) {
        return 0;
    }
    vec3 lightTangent = normalize(cross(L, vec3(0, 1, 0)));
    vec3 lightBitangent = normalize(cross(lightTangent, L));
    float numShadows = 0;
    for(int i = 0; i < numSamples; i++) {
        vec2 rng = BlueNoiseSample(numSamples + i).rg;
        vec2 diskSample = DiskSample(rng, radius);
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

float TraceAORays(vec3 fragPos, vec3 normal) {
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
        vec2 rng = BlueNoiseSample(scene.aoNumSamples + i).rg;
        vec3 randomVec = HemisphereSample(rng);
        vec3 direction = tangent*randomVec.x + bitangent*randomVec.y + normal*randomVec.z;
        // Ray Query for shadow
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos, tMin, direction, tMax);

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