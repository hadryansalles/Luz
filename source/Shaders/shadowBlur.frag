#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform BlurConstants {
    int imageRID;
    int depthRID;
    int normalRID;
    int steps;
    vec2 delta;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out float outValue;

#include "base.glsl"

void main() {
    outValue = 0;
    float sz = steps;
    float acc = 0;
    float centerShadow = texture(imageAttachs[imageRID], fragTexCoord).r;
    float centerDepth = texture(imageAttachs[depthRID], fragTexCoord).r;
    vec3 centerNormal = texture(imageAttachs[normalRID], fragTexCoord).rgb;
    float lastShadow = centerShadow;
    float lastLastShadow = centerShadow;
    if(sz == 0) {
        outValue = texture(imageAttachs[imageRID], fragTexCoord).r;
    } else {
        for(float i = -sz; i <= sz; i++) {
            vec2 coord = fragTexCoord + delta*i;
            float depth = texture(imageAttachs[depthRID], coord).r;
            vec3 normal = texture(imageAttachs[normalRID], coord).rgb;
            float shadow = texture(imageAttachs[imageRID], coord).r;
            float weight = 1.0;
            if(abs(depth-centerDepth) < 0.00001 && dot(normal, centerNormal) > 0.98) {
                outValue += weight*shadow;
                acc += weight;
            }
            lastLastShadow = lastShadow;
            lastShadow = shadow;
        }
        outValue /= acc;
    }
}