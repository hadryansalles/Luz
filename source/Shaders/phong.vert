#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// some data types occupy more than one location
// like the dvec3 is 64 bit, any attribute after
// it must have an index at least 2 higher

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 0) uniform TransformUBO {
    mat4 model;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    fragPos = transform.model * vec4(inPosition, 1.0);
    gl_Position = scene.proj * scene.view * fragPos;
    // fragNormal = vec3(inverse(transform.model)*vec4(inNormal, 0.0));
    fragNormal = vec3(transpose(inverse(transform.model))*vec4(inNormal, 1.0));
    fragTexCoord = inTexCoord;
}