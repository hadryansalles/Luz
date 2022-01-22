#version 450

const vec3 QUAD[6] = {
    vec3(-1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3( 1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0)
};

layout(location = 0) out vec2 fragPos;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    vec3 fragPos3 = QUAD[gl_VertexIndex];
    fragPos = fragPos3.xy;
    fragTexCoord = (fragPos3*0.5 + 0.5).xy;
    gl_Position = vec4(fragPos3, 1.0);
}