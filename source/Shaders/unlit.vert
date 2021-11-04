#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

// some data types occupy more than one location
// like the dvec3 is 64 bit, any attribute after
// it must have an index at least 2 higher

layout(set = 0, binding = 0) uniform TransformUBO {
    mat4 model;
} transform;

layout(set = 2, binding = 1) uniform SceneBuffer {
    mat4 view;
    mat4 proj;
} sceneBuffers[];

#define SCENE_BUFFER_INDEX 0
#define scene sceneBuffers[numFrames*SCENE_BUFFER_INDEX + frameID]

layout(push_constant) uniform PushConstant {
    int frameID;
    int numFrames;
    int textureID;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = scene.proj * scene.view * transform.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}