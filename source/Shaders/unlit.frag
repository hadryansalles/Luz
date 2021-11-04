#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 color;
} material;

layout(set = 2, binding = 0) uniform sampler2D colorTextures[];

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

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = material.color*texture(colorTextures[textureID], fragTexCoord);
}