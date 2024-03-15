#version 460

#extension GL_GOOGLE_include_directive : enable

#include "LuzCommon.h"

layout(push_constant) uniform Constants {
    int sceneBufferIndex;
    int modelBufferIndex;
    int modelID;
    int lightIndex;
};

void main() {
}