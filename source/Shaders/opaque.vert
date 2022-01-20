#version 460

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform ConstantsBlock {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int frame;
};

#include "base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec4 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragTangent;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out mat3 fragTBN;

void main() {
    fragPos = model.modelMat * vec4(inPosition, 1.0);
    gl_Position = scene.projView * fragPos;
    fragTexCoord = inTexCoord;
    mat4 transposeInverseModel = transpose(inverse(model.modelMat));
    fragTangent = normalize(vec3(transposeInverseModel*vec4(inTangent.xyz, 0.0)));
    fragNormal = normalize(vec3(transposeInverseModel*vec4(inNormal, 0.0)));
    fragTangent = normalize(fragTangent - dot(fragTangent, fragNormal) * fragNormal);
    vec3 B = cross(fragNormal, fragTangent)*inTangent.w;
    fragTBN = mat3(fragTangent, B, fragNormal);
}