#version 460

#extension GL_EXT_ray_tracing : require

struct hitPayload {
    vec3 color;
};

layout(location = 0) rayPayloadInEXT hitPayload prd;

void main() {
    prd.color = vec3(0, gl_HitTEXT/100, 0);
}