#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <string>
#include <filesystem>
#include <map>

struct GLFWwindow;

namespace vkw {

using Flags = uint32_t;

enum Memory {
    GPU = 0x00000001,
    CPU = 0x00000002 | 0x00000004,
};
using MemoryFlags = Flags;

namespace BufferUsage {
    enum {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        UniformTexel = 0x00000004,
        StorageTexel = 0x00000008,
        Uniform = 0x00000010,
        Storage = 0x00000020,
        Index = 0x00000040,
        Vertex = 0x00000080,
        Indirect = 0x00000100,
        Address = 0x00020000,
        VideoDecodeSrc = 0x00002000,
        VideoDecodeDst = 0x00004000,
        TransformFeedback = 0x00000800,
        TransformFeedbackCounter = 0x00001000,
        ConditionalRendering = 0x00000200,
        AccelerationStructureInput = 0x00080000,
        AccelerationStructure = 0x00100000,
        ShaderBindingTable = 0x00000400,
        SamplerDescriptor = 0x00200000,
        ResourceDescriptor = 0x00400000,
        PushDescriptors = 0x04000000,
        MicromapBuildInputReadOnly = 0x00800000,
        MicromapStorage = 0x01000000,
    };
}
using BufferUsageFlags = Flags;

enum Format {
    RGBA8_unorm = 37,
    BGRA8_unorm = 44,
    RG32_sfloat = 103,
    RGB32_sfloat = 106,
    RGBA32_sfloat = 109,
    D32_sfloat = 126,
    D24_unorm_S8_uint = 129,
};

namespace ImageUsage {
    enum {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthAttachment = 0x00000020,
    };
}
using ImageUsageFlags = Flags;

namespace Aspect {
    enum {
        Color = 1,
        Depth = 2,
        Stencil = 4,
    };
}
using AspectFlags = Flags;

namespace Layout {
    enum ImageLayout {
        Undefined = 0,
        General = 1,
        ColorAttachment = 2,
        DepthStencilAttachment = 3,
        DepthStencilRead = 4,
        ShaderRead = 5,
        TransferSrc = 6,
        TransferDst = 7,
        DepthReadStencilAttachment = 1000117000,
        DepthAttachmentStencilRead = 1000117001,
        DepthAttachment = 1000241000,
        DepthRead = 1000241001,
        StencilAttachment = 1000241002,
        StencilRead = 1000241003,
        Read = 1000314000,
        Attachment = 1000314001,
        Present = 1000001002,
    };
}

struct BufferResource;
struct ImageResource;
struct PipelineResource;
struct TLASResource;
struct BLASResource;

struct Buffer {
    std::shared_ptr<BufferResource> resource;
    uint32_t size;
    BufferUsageFlags usage;
    MemoryFlags memory;
    uint32_t RID();
};

struct Image {
    std::shared_ptr<ImageResource> resource;
    uint32_t width = 0;
    uint32_t height = 0;
    ImageUsageFlags usage;
    Format format;
    Layout::ImageLayout layout;
    AspectFlags aspect;
    uint32_t RID();
    ImTextureID ImGuiRID();
};

enum Queue {
    Graphics = 0,
    Compute = 1,
    Transfer = 2,
    Count = 3,
};

struct ImageDesc {
    uint32_t width;
    uint32_t height;
    Format format;
    ImageUsageFlags usage;
    std::string name = "";
};

namespace PipelinePoint {
    enum Point {
        Graphics = 0,
        Compute = 1,
    };
}

namespace ShaderStage {
    enum Stage {
        Vertex = 0x00000001,
        Geometry = 0x00000008,
        Fragment = 0x00000010,
        Compute = 0x00000020,
        AllGraphics = 0x0000001F,
        All = 0x7FFFFFFF,
    };
}

struct Pipeline {
    struct Stage {
        ShaderStage::Stage stage;
        std::filesystem::path path;
    };
    PipelinePoint::Point point;
    std::shared_ptr<PipelineResource> resource;
    std::vector<Stage> stages;
    std::vector<std::vector<char>> stageBytes;
};

struct PipelineDesc {
    PipelinePoint::Point point;
    std::vector<Pipeline::Stage> stages;
    std::string name = "";
    std::vector<Format> vertexAttributes;
    std::vector<Format> colorFormats;
    bool useDepth = false;
    Format depthFormat;
};

struct BLAS {
    std::shared_ptr<BLASResource> resource;
};

struct BLASInstance {
    BLAS blas;
    glm::mat4 modelMat;
    uint32_t customIndex;
};

struct BLASDesc {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexStride;
    std::string name;
};

struct TLAS {
    std::shared_ptr<TLASResource> resource;
    uint32_t RID();
};

Buffer CreateBuffer(uint32_t size, BufferUsageFlags usage, MemoryFlags memory = Memory::GPU, const std::string& name = "");
Image CreateImage(const ImageDesc& desc);
Pipeline CreatePipeline(const PipelineDesc& desc);
TLAS CreateTLAS(uint32_t maxInstances, const std::string& name);
BLAS CreateBLAS(const BLASDesc& desc);

void AcquireImage();
void SubmitAndPresent();
bool GetSwapChainDirty();

void GetTimeStamps(std::map<std::string, float>& timeTable);

void CmdCopy(Buffer& dst, void* data, uint32_t size, uint32_t dstOfsset = 0);
void CmdCopy(Buffer& dst, Buffer& src, uint32_t size, uint32_t dstOffset = 0, uint32_t srcOffset = 0);
void CmdCopy(Image& dst, void* data, uint32_t size);
void CmdCopy(Image& dst, Buffer& src, uint32_t size, uint32_t srcOffset = 0);
void CmdBarrier(Image& img, Layout::ImageLayout layout);
void CmdBarrier();
void CmdBeginRendering(const std::vector<Image>& colorAttachs, Image depthAttach = {});
void CmdEndRendering();
void CmdBeginPresent();
void CmdEndPresent();
void CmdBindPipeline(Pipeline& pipeline);
void CmdPushConstants(void* data, uint32_t size);
void CmdBuildBLAS(BLAS& blas);
void CmdBuildTLAS(TLAS& tlas, const std::vector<BLASInstance>& instances);
void CmdDrawMesh(Buffer& vertexBuffer, Buffer& indexBuffer, uint32_t indexCount);
void CmdDrawPassThrough();
void CmdDrawImGui(ImDrawData* data);

int CmdBeginTimeStamp(const std::string& name);
void CmdEndTimeStamp(int timeStampIndex);

void BeginCommandBuffer(Queue queue);
void EndCommandBuffer();
void WaitQueue(Queue queue);
void WaitIdle();

void Init(GLFWwindow* window, uint32_t width, uint32_t height);
void InitImGui();
void OnSurfaceUpdate(uint32_t width, uint32_t height);
void Destroy();

template<typename T>
void CmdTimeStamp(const std::string& name, T callback) {
    int id = CmdBeginTimeStamp(name);
    callback();
    CmdEndTimeStamp(id);
}

}