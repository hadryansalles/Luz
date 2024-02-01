#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(push_constant) uniform PresentConstants {
    int presentMode;
    int lightRID;
    int albedoRID;
    int normalRID;
    int materialRID;
    int emissionRID;
    int depthRID;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float LinearizeDepth(float depth) {
    float near = 0.01;
    float far = 5.0;
    return (2.0*near)/(far+near - depth*(far-near));
}

void main() {
    int imageType = presentMode;
    vec2 fragCoord = fragTexCoord;
    if(presentMode == 6) {
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
    int imageRID = lightRID;
    if(imageType == 1) {
        imageRID = albedoRID;
    } else if(imageType == 2) {
        imageRID = normalRID;
    } else if(imageType == 3) {
        imageRID = materialRID;
    } else if(imageType == 4) {
        imageRID = emissionRID;
    } else if(imageType == 5) {
        imageRID = depthRID;
    }    
    vec4 value = texture(textures[imageRID], fragCoord);
    if(imageType == 2) {
        value = (value + 1.0)/2.0;
    } else if(imageType == 5) {
        value = LinearizeDepth(value.r).xxxx;
    }
    outColor = vec4(value.rgb, 1.0);
}
