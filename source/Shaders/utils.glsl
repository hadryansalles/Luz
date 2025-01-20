vec3 DepthToWorld(vec2 screenPos, float depth) {
    vec4 clipSpacePos = vec4(screenPos*2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePos = scene.inverseProj*clipSpacePos;
    viewSpacePos /= viewSpacePos.w;
    vec4 worldSpacePos = scene.inverseView*viewSpacePos;
    return worldSpacePos.xyz;
}

float MitchellFilter(float x) {
    const float B = 1.0/3.0;
    const float C = 1.0/3.0;
    float x2 = x*x;
    float x3 = x2*x;
    return (6.0 - 2.0*B)*x3 - (6.0 - 2.0*B - 3.0*C)*x2 + 1.0;
}

vec4 SampleTextureCatmullRom(sampler2D tex, vec2 uv) {
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 texPos0 = texPos1 - 1.0;
    vec2 texPos3 = texPos1 + 2.0;
    vec2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    vec4 result = vec4(0.0);
    result += texture(tex, vec2(texPos0.x, texPos0.y)) * w0.x * w0.y;
    result += texture(tex, vec2(texPos12.x, texPos0.y)) * w12.x * w0.y;
    result += texture(tex, vec2(texPos3.x, texPos0.y)) * w3.x * w0.y;

    result += texture(tex, vec2(texPos0.x, texPos12.y)) * w0.x * w12.y;
    result += texture(tex, vec2(texPos12.x, texPos12.y)) * w12.x * w12.y;
    result += texture(tex, vec2(texPos3.x, texPos12.y)) * w3.x * w12.y;

    result += texture(tex, vec2(texPos0.x, texPos3.y)) * w0.x * w3.y;
    result += texture(tex, vec2(texPos12.x, texPos3.y)) * w12.x * w3.y;
    result += texture(tex, vec2(texPos3.x, texPos3.y)) * w3.x * w3.y;

    return result;
}

vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
    vec3 r = q - p;
    vec3 rmax = aabb_max - p;
    vec3 rmin = aabb_min - p;

    const float eps = 1e-7;

    if (r.x > rmax.x + eps)
        r *= (rmax.x / r.x);
    if (r.y > rmax.y + eps)
        r *= (rmax.y / r.y);
    if (r.z > rmax.z + eps)
        r *= (rmax.z / r.z);

    if (r.x < rmin.x - eps)
        r *= (rmin.x / r.x);
    if (r.y < rmin.y - eps)
        r *= (rmin.y / r.y);
    if (r.z < rmin.z - eps)
        r *= (rmin.z / r.z);

    return p + r;
}

float rcp(float value) {
    return 1.0 / value;
}

float Luminance(vec3 color) {
    return dot(color, vec3(0.2127, 0.7152, 0.0722));
}

vec4 BlueNoiseSample(int i, vec2 fragCoord) {
    vec2 blueNoiseSize = textureSize(textures[scene.blueNoiseTexture], 0);
    ivec2 fragUV = ivec2(mod(fragCoord, blueNoiseSize));
    return fract(texelFetch(textures[scene.blueNoiseTexture], fragUV, 0) + GOLDEN_RATIO*(128*i + ctx.frame%128));
}

float SampleShadowMapPCF(LightBlock light, vec3 fragPos, float radius, vec2 fragCoord) {
    int numSamples = scene.pcfSamples;
    if (light.type == LUZ_LIGHT_TYPE_DIRECTIONAL || light.type == LUZ_LIGHT_TYPE_SUN) {
        vec4 fragInLight = (light.viewProj[0] * vec4(fragPos, 1));
        vec2 baseUV = (fragInLight.xy * 0.5 + vec2(0.5f, 0.5f));
        
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(textures[light.shadowMap], 0);
        float screenRadius = radius / fragInLight.w;
        
        for(int i = 0; i < numSamples; i++) {
            vec2 blueNoise = BlueNoiseSample(i, fragCoord).rg * 4.0 - 2.0;
            vec2 offset = blueNoise * texelSize * screenRadius;
            vec2 uv = baseUV + offset;
            float shadowDepth = texture(textures[light.shadowMap], uv).r;
            shadow += fragInLight.z >= shadowDepth ? 1.0 : 0.0;
        }
        
        return shadow / float(numSamples);
    } else {
        vec3 lightToFrag = fragPos - light.position;
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(cubeTextures[light.shadowMap], 0);
        float distanceToReceiver = length(lightToFrag);
        vec3 lightDir = normalize(lightToFrag);
        float distanceToOccluder = texture(cubeTextures[light.shadowMap], lightDir).r * light.zFar;
        float penumbraWidth = (light.radius * distanceToReceiver) / (distanceToOccluder);
        float screenRadius = penumbraWidth * 100.0;

        for(int i = 0; i < numSamples; i++) {
            vec2 blueNoise = BlueNoiseSample(i, fragCoord).rg * 4.0 - 2.0;
            vec2 offset = blueNoise * texelSize * screenRadius;
            vec3 offsetDir = normalize(lightToFrag + vec3(offset, 0));
            float shadowDepth = texture(cubeTextures[light.shadowMap], offsetDir).r;
            shadow += distanceToReceiver >= (shadowDepth * light.zFar) ? 1.0 : 0.0;
        }
        
        return shadow / float(numSamples);
    }
}