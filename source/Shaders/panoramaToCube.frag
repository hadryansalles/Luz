#version 450

#include "base.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PanoramaToCubeConstants {
    int panoramaRID;
    int face;
};

vec3 UVToPosXYZ(int face, vec2 uv) {
    if(face == 0) { // right
        return vec3(1.f, uv.y, 1.0-uv.x);
    } else if(face == 1) { // left
        return vec3(0, uv.y, uv.x);
    } else if(face == 2) { // top
        return vec3(uv.x, 0.0f, uv.y);
    } else if(face == 3) { // bottom
        return vec3(uv.x, 1.f, 1.0-uv.y);
    } else if(face == 4) { // font
        return vec3(uv.x, uv.y, 1.f);
    } else { //back
        return vec3(1.0f-uv.x, uv.y, 0.0f);
    }
}

vec3 UVToXYZ(int face, vec2 uv) {
    if(face == 0) { // right
        return vec3(1.f, uv.y, -uv.x);
    } else if(face == 1) { // left
        return vec3(-1.f, uv.y, uv.x);
    } else if(face == 2) { // top
        return vec3(uv.x, -1.f, uv.y);
    } else if(face == 3) { // bottom
        return vec3(uv.x, 1.f, -uv.y);
    } else if(face == 4) { // font
        return vec3(uv.x, uv.y, 1.f);
    } else { //back
        return vec3(-uv.x, uv.y, -1.f);
    }
}

vec2 DirToUV(vec3 dir) {
    // return vec2(0.5f + 0.5f * atan(dir.z, dir.x) / PI, 1.f - acos(dir.y) / PI);
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= vec2(0.159, 0.3183);
    uv += 0.5;
    return uv;
}

vec3 PanoramaToCubeMap(int face, vec2 texCoord) {
    vec2 texCoordNew = texCoord*2.0f - 1.0f;
    vec3 cartesian = UVToXYZ(face, texCoordNew); 
    // cartesian = cartesian*0.5;
    cartesian = normalize(cartesian);
    // float theta = atan(cartesian.z, cartesian.x);
    // float phi = acos(cartesian.y/length(cartesian));
    // float u = theta/(2*PI) + 1.0;
    // float v = phi/(PI) + 1.0;
    // return texture(textures[panoramaRID], vec2(u,v)).rgb;
    return texture(textures[panoramaRID], DirToUV(cartesian)).rgb;
}

void main() {
    outColor = vec4(PanoramaToCubeMap(face, fragTexCoord), 1.0);
    // outColor = vec4(UVToPosXYZ(face, fragTexCoord), 1.0);
    // outColor = vec4(1,0,1,1);
}