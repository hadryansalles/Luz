#pragma once

#include <memory>
#include <stdint.h>
#include <string>

struct GLFWwindow;

namespace vk {

enum class Memory {
    GPU = 0x00000001,
    CPU = 0x00000002 | 0x00000004,
};

enum Usage {
    TransferSrc = 0x00000001,
    TransferDst = 0x00000002,
    UniformTexel = 0x00000004,
    StorageTexel = 0x00000008,
    Uniform = 0x00000010,
    Storage = 0x00000020,
    Index = 0x00000040,
    Vertex = 0x00000080,
    Indirect = 0x00000100,
    ShaderDeviceAdress = 0x00020000,
    VideoDecodeSrc = 0x00002000,
    VideoDecodeDst = 0x00004000,
    TransformFeedback = 0x00000800,
    TransformFeedbackCounter = 0x00001000,
    ConditionalRendering = 0x00000200,
    AccelerationStructureBuildInputReadOnly = 0x00080000,
    AccelerationStructureStorage = 0x00100000,
    ShaderBindingTable = 0x00000400,
    SamplerDescriptor = 0x00200000,
    ResourceDescriptor = 0x00400000,
    PushDescriptors = 0x04000000,
    MicromapBuildInputReadOnly = 0x00800000,
    MicromapStorage = 0x01000000,
};

struct BufferResource;

struct Buffer {
    std::shared_ptr<BufferResource> resource;
    const uint32_t size;
    const Usage usage;
    const Memory memory;
    uint32_t RID();
};

Buffer CreateBuffer(uint32_t size, Usage usage, Memory memory, const std::string& name = "");

void Init(GLFWwindow* window);
void Destroy();
void* Map(Buffer buffer);
void Unmap(Buffer buffer);

}