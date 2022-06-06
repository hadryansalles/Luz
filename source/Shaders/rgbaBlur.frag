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

layout(location = 0) out vec4 outValue;

#include "base.glsl"

void main() {
    outValue = vec4(0, 0, 0, 0);
    float sz = steps;
    float acc = 0;
    vec4 centerValue = texture(imageAttachs[imageRID], fragTexCoord);
    float centerDepth = texture(imageAttachs[depthRID], fragTexCoord).r;
    vec3 centerNormal = texture(imageAttachs[normalRID], fragTexCoord).rgb;
    vec4 lastValue = centerValue;
    vec4 lastLastValue = centerValue;
    if(sz == 0) {
        outValue = texture(imageAttachs[imageRID], fragTexCoord);
    } else {
        float importance = 0.0f;
        for(float i = -sz; i <= sz; i++) {
            vec2 coord = fragTexCoord + delta*i;
            float depth = texture(imageAttachs[depthRID], coord).r;
            vec3 normal = texture(imageAttachs[normalRID], coord).rgb;
            vec4 value = texture(imageAttachs[imageRID], coord);
            // float weight = 1.0;
            float depthImportance = clamp(1.0/abs(centerDepth-depth), 0.0, 1.0);
            outValue += value;
            // outValue += depthImportance*value;
            // importance += depthImportance;
            acc += 1.0;
            lastLastValue = lastValue;
            lastValue = value;
        }
        outValue /= acc;
        // outValue /= importance;
    }
}