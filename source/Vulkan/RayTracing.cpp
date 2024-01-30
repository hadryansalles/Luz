#include "Luzpch.hpp"
#include "RayTracing.hpp"

#include "Scene.hpp"
#include "AssetManager.hpp"
#include "FileManager.hpp"
#include "VulkanLayer.h"
#include "common.h"

#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

namespace RayTracing {

struct BLASInput {
    std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
    VkBuildAccelerationStructureFlagsKHR flags{};
};

struct AccelerationStructure {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    vkw::Buffer buffer;
};

struct BuildAccelerationStructure {
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
    AccelerationStructure as;
};

struct RayTracingContext {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    bool useRayTracing = true;
    bool recreateTLAS = false;
    bool autoUpdateTLAS = true;

    VkViewport viewport;

    std::vector<AccelerationStructure> BLAS;
    AccelerationStructure TLAS;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties;

    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
};

struct RayTracingPushConstants {
    glm::mat4 view;
    glm::mat4 proj;
    //glm::mat4 viewProj;
    //glm::mat4 projInverse;
    //glm::mat4 viewInverse;
    //glm::vec4 clearColor;
    //glm::vec3 lightPosition;
    //float lightIntensity;
};

RayTracingContext ctx;

void CreateBLAS(std::vector<RID>& meshes) {
    LUZ_PROFILE_FUNC();
    VkDevice device = vkw::ctx().device;

    std::vector<BLASInput> blasInputs;
    blasInputs.reserve(meshes.size());

    for (RID meshID : meshes) {
        MeshResource& mesh = AssetManager::meshes[meshID];

        // BLAS builder requires raw device addresses.
        VkBufferDeviceAddressInfo vertexBufferInfo{};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        vertexBufferInfo.buffer = mesh.vertexBuffer.GetBuffer();
        vertexBufferInfo.pNext = nullptr;
        VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device, &vertexBufferInfo);
        VkBufferDeviceAddressInfo indexBufferInfo{};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        indexBufferInfo.buffer = mesh.indexBuffer.GetBuffer();
        VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device, &indexBufferInfo);

        u32 maxPrimitiveCount = mesh.indexCount / 3;

        // Describe buffer as array of VertexObj.
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = vertexAddress;
        triangles.vertexStride = sizeof(MeshVertex);
        triangles.maxVertex = mesh.vertexCount;
        // Describe index data (32-bit unsigned int)
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = indexAddress;
        // Indicate identity transform by setting transformData to null device pointer.
        // triangles.transformData = {};

        // Identify the above data as containing opaque triangles.
        VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        asGeom.geometry.triangles = triangles;

        // The entire array will be used to build the BLAS.
        VkAccelerationStructureBuildRangeInfoKHR offset;
        offset.firstVertex = 0;
        offset.primitiveCount = maxPrimitiveCount;
        offset.primitiveOffset = 0;
        offset.transformOffset = 0;

        // Our blas is made from only one geometry, but could be made of many geometries
        BLASInput input;
        input.asGeometry.emplace_back(asGeom);
        input.asBuildOffsetInfo.emplace_back(offset);

        blasInputs.emplace_back(input);
    }

    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    u32 nbBlas = (u32)blasInputs.size();
    VkDeviceSize asTotalSize = 0;    // Memory size of all allocated BLAS
    VkDeviceSize maxScratchSize = 0; // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for (uint32_t idx = 0; idx < nbBlas; idx++) {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        // Other information will be filled in the createBlas (see #2)
        buildAs[idx].buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildAs[idx].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildAs[idx].buildInfo.flags = blasInputs[idx].flags | flags;
        buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(blasInputs[idx].asGeometry.size());
        buildAs[idx].buildInfo.pGeometries = blasInputs[idx].asGeometry.data();

        // Build range information
        buildAs[idx].rangeInfo = blasInputs[idx].asBuildOffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(blasInputs[idx].asBuildOffsetInfo.size());
        for (auto tt = 0; tt < blasInputs[idx].asBuildOffsetInfo.size(); tt++)
            maxPrimCount[tt] = blasInputs[idx].asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
        buildAs[idx].sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        ctx.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildAs[idx].buildInfo, maxPrimCount.data(), &buildAs[idx].sizeInfo);

        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
    }

    vkw::Buffer scratchBuffer = vkw::CreateBuffer(maxScratchSize, vkw::BufferUsage::Storage, vkw::Memory::GPU);
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = scratchBuffer.GetBuffer();
    VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &bufferInfo);

    std::vector<uint32_t> indices; // Indices of the BLAS to create
    VkDeviceSize          batchSize = 0;
    VkDeviceSize          batchLimit = 256'000'000; // 256 MB
    for (uint32_t idx = 0; idx < nbBlas; idx++) {
        indices.push_back(idx);
        batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || idx == nbBlas - 1) {
            // todo: keep an eye, maybe move to graphics
            vkw::BeginCommandBuffer(vkw::Queue::Compute);
            VkCommandBuffer cmdBuf = vkw::ctx().GetCurrentCommandBuffer();
            {
                for(const auto& idx : indices)
                {
                    // Actual allocation of buffer and acceleration structure.
                    buildAs[idx].as.buffer = vkw::CreateBuffer(buildAs[idx].sizeInfo.accelerationStructureSize, vkw::BufferUsage::AccelerationStructure, vkw::Memory::GPU);

                    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize; // Will be used to allocate memory.
                    createInfo.buffer = buildAs[idx].as.buffer.GetBuffer();
                    ctx.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &buildAs[idx].as.accel);

                    // BuildInfo #2 part
                    buildAs[idx].buildInfo.dstAccelerationStructure  = buildAs[idx].as.accel; // Setting where the build lands
                    buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress; // All build are using the same scratch buffer

                    // Building the bottom-level-acceleration-structure
                    ctx.vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].buildInfo, &buildAs[idx].rangeInfo);

                    // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
                    // is finished before starting the next one.
                    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                     VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
                }
            }
            vkw::EndCommandBuffer();
            vkw::WaitQueue(vkw::Queue::Compute);
            // Reset
            batchSize = 0;
            indices.clear();
        }
    }

    // ctx.BLAS.clear();
    for(auto& b : buildAs) {
        ctx.BLAS.emplace_back(b.as);
    }
    scratchBuffer = {};

    LOG_INFO("Created BLAS.");
}

void CreateTLAS() {
    LUZ_PROFILE_FUNC();
    if (!ctx.autoUpdateTLAS || !ctx.useRayTracing) {
        return;
    }
    auto device = vkw::ctx().device;
    bool update = ctx.TLAS.accel != VK_NULL_HANDLE;

    if (ctx.recreateTLAS) {
        update = false;
        if (ctx.TLAS.accel != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            ctx.recreateTLAS = false;
            update = false;
            ctx.vkDestroyAccelerationStructureKHR(device, ctx.TLAS.accel, nullptr);
            ctx.TLAS.accel = VK_NULL_HANDLE;
            ctx.TLAS.buffer = {};
        }
    }

    const std::vector<Model*>& models = Scene::modelEntities;
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(models.size());
    for (int i = 0; i < models.size(); i++) {
        Model* model = models[i];

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = ctx.BLAS[model->mesh].accel;

        glm::mat4 modelMat = model->transform.GetMatrix();
        VkTransformMatrixKHR transform{};
        transform.matrix[0][0] = modelMat[0][0];
        transform.matrix[0][1] = modelMat[1][0];
        transform.matrix[0][2] = modelMat[2][0];
        transform.matrix[0][3] = modelMat[3][0];
        transform.matrix[1][0] = modelMat[0][1];
        transform.matrix[1][1] = modelMat[1][1];
        transform.matrix[1][2] = modelMat[2][1];
        transform.matrix[1][3] = modelMat[3][1];
        transform.matrix[2][0] = modelMat[0][2];
        transform.matrix[2][1] = modelMat[1][2];
        transform.matrix[2][2] = modelMat[2][2];
        transform.matrix[2][3] = modelMat[3][2];

        VkAccelerationStructureInstanceKHR rayInstance{};
        rayInstance.transform = transform;
        rayInstance.instanceCustomIndex = model->id;
        rayInstance.accelerationStructureReference = ctx.vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
        rayInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        rayInstance.mask = 0xFF;
        rayInstance.instanceShaderBindingTableRecordOffset = 0;
        instances.emplace_back(rayInstance);
    }

    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    DEBUG_ASSERT(ctx.TLAS.accel == VK_NULL_HANDLE || update, "TLAS already created!");
    u32 countInstance = (u32)instances.size();

    vkw::Buffer instancesBuffer = vkw::CreateBuffer(sizeof(VkAccelerationStructureInstanceKHR) * instances.size(), vkw::BufferUsage::AccelerationStructureInput, vkw::Memory::GPU);
    vkw::BeginCommandBuffer(vkw::Queue::Graphics);
    vkw::CmdCopy(instancesBuffer, instances.data(), instancesBuffer.size);
    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Queue::Graphics);

    vkw::BeginCommandBuffer(vkw::Graphics);
    VkCommandBuffer commandBuffer = vkw::ctx().GetCurrentCommandBuffer();

    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = instancesBuffer.GetBuffer();
    VkDeviceAddress instancesBufferAddr = vkGetBufferDeviceAddress(device, &bufferInfo);

    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkw::Buffer scratchBuffer;

    // command create TLAS
    {
        bool motion = false;

        // Wraps a device pointer to the above uploaded instances.
        VkAccelerationStructureGeometryInstancesDataKHR instancesVk{};
        instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instancesVk.data.deviceAddress = instancesBufferAddr;

        // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
        VkAccelerationStructureGeometryKHR topASGeometry{};
        topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        topASGeometry.geometry.instances = instancesVk;

        // Find sizes
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.flags = flags;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &topASGeometry;
        buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        ctx.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &countInstance, &sizeInfo);

        if (!update) {
            ctx.TLAS.buffer = vkw::CreateBuffer(sizeInfo.accelerationStructureSize, vkw::BufferUsage::AccelerationStructure, vkw::Memory::GPU);

            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.size = sizeInfo.accelerationStructureSize;
            createInfo.buffer = ctx.TLAS.buffer.GetBuffer();
            ctx.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &ctx.TLAS.accel);

            VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructure{};
            descriptorAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            descriptorAccelerationStructure.accelerationStructureCount = 1;
            descriptorAccelerationStructure.pAccelerationStructures = &ctx.TLAS.accel;

            std::vector<VkWriteDescriptorSet> writes;

            VkWriteDescriptorSet writeBindlessAccelerationStructure{};
            writeBindlessAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeBindlessAccelerationStructure.pNext = &descriptorAccelerationStructure;
            writeBindlessAccelerationStructure.dstSet = vkw::ctx().bindlessDescriptorSet;
            writeBindlessAccelerationStructure.dstBinding = LUZ_BINDING_TLAS;
            writeBindlessAccelerationStructure.dstArrayElement = 0;
            writeBindlessAccelerationStructure.descriptorCount = 1;
            writeBindlessAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            writes.push_back(writeBindlessAccelerationStructure);

            vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
            DEBUG_TRACE("Update descriptor sets in CreateTLAS!");
        }

        scratchBuffer = vkw::CreateBuffer(sizeInfo.buildScratchSize, vkw::BufferUsage::Address | vkw::BufferUsage::Storage, vkw::Memory::GPU);

        VkBufferDeviceAddressInfo scratchInfo{};
        scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchInfo.buffer = scratchBuffer.GetBuffer();
        VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &scratchInfo);

        // Update build information
        buildInfo.srcAccelerationStructure = ctx.TLAS.accel;
        buildInfo.dstAccelerationStructure = ctx.TLAS.accel;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        // Build Offsets info: n instances
        VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ countInstance, 0, 0, 0 };
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

        // Build the TLAS
        ctx.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildOffsetInfo);
    }

    vkw::EndCommandBuffer();
    vkw::WaitQueue(vkw::Graphics);
    scratchBuffer = {};
    instancesBuffer = {};
}

void Create() {
    LUZ_PROFILE_FUNC();
    VkDevice device = vkw::ctx().device;
    ctx.vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    ctx.vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    ctx.vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    ctx.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    ctx.vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    ctx.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    ctx.vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
    ctx.vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
}

void Destroy() {
    VkDevice device = vkw::ctx().device;
    for (int i = 0; i < ctx.BLAS.size(); i++) {
        ctx.vkDestroyAccelerationStructureKHR(device, ctx.BLAS[i].accel, nullptr);
        ctx.BLAS[i].buffer = {};
    }
    ctx.BLAS.clear();
    ctx.vkDestroyAccelerationStructureKHR(device, ctx.TLAS.accel, nullptr);
    ctx.TLAS.buffer = {};
    ctx.TLAS.accel = VK_NULL_HANDLE;
}

void OnImgui() {
}

void SetRecreateTLAS() {
    ctx.recreateTLAS = true;
}

}
