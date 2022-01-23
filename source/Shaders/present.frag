#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(push_constant) uniform PresentConstants {
    int imageRID;
    int imageType;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float LinearizeDepth(float depth) {
    float near = 0.01;
    float far = 5.0;
    return (2.0*near)/(far+near - depth*(far-near));
}

void main() {
    vec4 value = texture(imageAttachs[imageRID], fragTexCoord);
    if(imageType == 2) {
        value = (value + 1.0)/2.0;
    } else if(imageType == 5) {
        value = LinearizeDepth(value.r).xxxx;
    }
    outColor = vec4(value.rgb, 1.0);
}
