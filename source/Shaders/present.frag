#version 450

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    ComposeConstants ctx;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float LinearizeDepth(float depth) {
    float near = 0.01;
    float far = 5.0;
    return (2.0*near)/(far+near - depth*(far-near));
}

vec3 Tonemap(vec3 color) {
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 
    return color;
}

vec3 ExposureToneMapping(vec3 color, float exposure) {
    return vec3(1.0) - exp(-color * exposure);
}

vec3 TonemapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    color = (color * (a * color + b)) / (color * (c * color + d) + e);
    return pow(color, vec3(1.0 / 2.2));
}

vec3 TonemapExposure(vec3 color, float exposure) {
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    return pow(mapped, vec3(1.0 / 2.2));
}

void main() {
    int imageType = ctx.imageType;
    vec2 fragCoord = fragTexCoord;
    if(ctx.imageType == 6) {
        fragCoord = vec2(fragTexCoord.x*3, fragTexCoord.y*2);
        if(fragTexCoord.y < 0.5) {
            if(fragTexCoord.x < 1.0/3.0) {
                imageType = 0;
            } else if(fragTexCoord.x < 2.0/3.0) {
                imageType = 1;
                fragCoord.x -= 1;
            } else {
                imageType = 2;
                fragCoord.x -= 2;
            }
        } else {
            fragCoord.y -= 1;
            if(fragTexCoord.x < 1.0/3.0) {
                imageType = 3;
            } else if(fragTexCoord.x < 2.0/3.0) {
                imageType = 4;
                fragCoord.x -= 1;
            } else {
                imageType = 5;
                fragCoord.x -= 2;
            }
        }
        fragCoord.x = 1.0/6.0 + fragCoord.x*2.0/3.0;
    }
    int imageRID = ctx.lightRID;
    if(imageType == 1) {
        imageRID = ctx.albedoRID;
    } else if(imageType == 2) {
        imageRID = ctx.normalRID;
    } else if(imageType == 3) {
        imageRID = ctx.materialRID;
    } else if(imageType == 4) {
        imageRID = ctx.emissionRID;
    } else if(imageType == 5) {
        imageRID = ctx.depthRID;
    }
    vec4 value = texture(textures[imageRID], fragCoord);
    vec4 debugColor = texture(textures[ctx.debugRID], fragCoord);
    if (imageType == 0) {
        vec3 color = ExposureToneMapping(value.rgb, ctx.exposure);
        // vec3 color = TonemapACES(value.rgb);
        // vec3 color = value.rgb;
        // color = color * (1.0 - debugColor.a) + debugColor.rgb * debugColor.a;
        value = vec4(color, 1.0);
    } else if(imageType == 2) {
        value = (value + 1.0)/2.0;
    } else if(imageType == 5) {
        value = LinearizeDepth(value.r).xxxx;
    }
    outColor = vec4(value.rgb, 1.0);
}