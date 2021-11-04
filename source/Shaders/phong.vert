#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform TransformUBO {
    mat4 model;
} transform;

layout(push_constant) uniform PushConstant {
    int frameID;
    int numFrames;
    int textureID;
};

layout(set = 2, binding = 1) uniform SceneBuffer {
    mat4 view;
    mat4 proj;
} sceneBuffers[];

#define SCENE_BUFFER_INDEX 0
#define scene sceneBuffers[numFrames*SCENE_BUFFER_INDEX + frameID]

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    fragPos = transform.model * vec4(inPosition, 1.0);
    gl_Position = scene.proj * scene.view * fragPos;
    fragNormal = normalize(vec3(transpose(inverse(transform.model))*vec4(inNormal, 1.0)));
    fragTexCoord = inTexCoord;
}