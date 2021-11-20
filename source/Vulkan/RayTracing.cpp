#include "Luzpch.hpp"
#include "RayTracing.hpp"

#include "Shader.hpp"
#include "LogicalDevice.hpp"
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "ImageManager.hpp"
#include "BufferManager.hpp"
#include "FileManager.hpp"
#include "SwapChain.hpp"
#include "PhysicalDevice.hpp"
#include "Texture.hpp"
#include "GraphicsPipelineManager.hpp"

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
    BufferResource buffer;
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
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

    bool useRayTracing = true;
    bool recreateTLAS = false;

    ImageResource image;
    VkSampler sampler;
    ImTextureID imguiTextureID;
    VkViewport viewport;

    std::vector<AccelerationStructure> BLAS;
    AccelerationStructure TLAS;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties;

    BufferResource SBTBuffer;
    VkStridedDeviceAddressRegionKHR rgenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR callRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};

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
    VkDevice device = LogicalDevice::GetVkDevice();

    std::vector<BLASInput> blasInputs;
    blasInputs.reserve(meshes.size());

    for (RID meshID : meshes) {
        MeshResource& mesh = AssetManager::meshes[meshID];

        // BLAS builder requires raw device addresses.
        VkBufferDeviceAddressInfo vertexBufferInfo{};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        vertexBufferInfo.buffer = mesh.vertexBuffer.buffer;
        vertexBufferInfo.pNext = nullptr;
        VkDeviceAddress vertexAddress = ctx.vkGetBufferDeviceAddressKHR(device, &vertexBufferInfo);
        VkBufferDeviceAddressInfo indexBufferInfo{};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        indexBufferInfo.buffer = mesh.indexBuffer.buffer;
        VkDeviceAddress indexAddress = ctx.vkGetBufferDeviceAddressKHR(device, &indexBufferInfo);

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

    BufferResource scratchBuffer;
    BufferDesc scratchBufferDesc;
    scratchBufferDesc.size = maxScratchSize;
    scratchBufferDesc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    scratchBufferDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    BufferManager::Create(scratchBufferDesc, scratchBuffer);
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = scratchBuffer.buffer;
    VkDeviceAddress scratchAddress = ctx.vkGetBufferDeviceAddressKHR(device, &bufferInfo);

    std::vector<uint32_t> indices; // Indices of the BLAS to create
    VkDeviceSize          batchSize = 0;
    VkDeviceSize          batchLimit = 256'000'000; // 256 MB
    for (uint32_t idx = 0; idx < nbBlas; idx++) {
        indices.push_back(idx);
        batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || idx == nbBlas - 1) {
            VkCommandBuffer cmdBuf = LogicalDevice::BeginSingleTimeCommands();
            {
                for(const auto& idx : indices)
                {
                    // Actual allocation of buffer and acceleration structure.
                    BufferDesc asBufferDesc;
                    asBufferDesc.size = buildAs[idx].sizeInfo.accelerationStructureSize;
                    asBufferDesc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
                    asBufferDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    BufferManager::Create(asBufferDesc, buildAs[idx].as.buffer);

                    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize; // Will be used to allocate memory.
                    createInfo.buffer = buildAs[idx].as.buffer.buffer;
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
            LogicalDevice::EndSingleTimeCommands(cmdBuf);
            // Reset
            batchSize = 0;
            indices.clear();
        }
    }

    // ctx.BLAS.clear();
    for(auto& b : buildAs) {
        ctx.BLAS.emplace_back(b.as);
    }
    BufferManager::Destroy(scratchBuffer);

    LOG_INFO("Created BLAS.");
}

void CreateTLAS() {
    LUZ_PROFILE_FUNC();
    if (!ctx.useRayTracing) {
        return;
    }
    auto device = LogicalDevice::GetVkDevice();
    bool update = ctx.TLAS.accel != VK_NULL_HANDLE;
    
    if (ctx.recreateTLAS) {
        vkDeviceWaitIdle(device);
        ctx.recreateTLAS = false;
        update = false;
        ctx.vkDestroyAccelerationStructureKHR(device, ctx.TLAS.accel, nullptr);
        ctx.TLAS.accel = VK_NULL_HANDLE;
        BufferManager::Destroy(ctx.TLAS.buffer);
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
        rayInstance.instanceCustomIndex = i;
        rayInstance.accelerationStructureReference = ctx.vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
        rayInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        rayInstance.mask = 0xFF;
        rayInstance.instanceShaderBindingTableRecordOffset = 0;
        instances.emplace_back(rayInstance);
    }

    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    DEBUG_ASSERT(ctx.TLAS.accel == VK_NULL_HANDLE || update, "TLAS already created!");
    u32 countInstance = (u32)instances.size();

    BufferDesc instancesBufferDesc;
    instancesBufferDesc.size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
    instancesBufferDesc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    instancesBufferDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    BufferResource instancesBuffer;
    BufferManager::CreateStaged(instancesBufferDesc, instancesBuffer, instances.data());

    VkCommandBuffer commandBuffer = LogicalDevice::BeginSingleTimeCommands();
    
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = instancesBuffer.buffer;
    VkDeviceAddress instancesBufferAddr = ctx.vkGetBufferDeviceAddressKHR(device, &bufferInfo);

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
    BufferResource scratchBuffer;

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
            BufferDesc asBufferDesc;
            asBufferDesc.size = sizeInfo.accelerationStructureSize;
            asBufferDesc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
            asBufferDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            BufferManager::Create(asBufferDesc, ctx.TLAS.buffer);

            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.size = sizeInfo.accelerationStructureSize;
            createInfo.buffer = ctx.TLAS.buffer.buffer;
            ctx.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &ctx.TLAS.accel);

            VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructure{};
            descriptorAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            descriptorAccelerationStructure.accelerationStructureCount = 1;
            descriptorAccelerationStructure.pAccelerationStructures = &ctx.TLAS.accel;

            std::vector<VkWriteDescriptorSet> writes;

            VkWriteDescriptorSet writeAccelerationStructure{};
            writeAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeAccelerationStructure.pNext = &descriptorAccelerationStructure;
            writeAccelerationStructure.dstSet = ctx.descriptorSet;
            writeAccelerationStructure.dstBinding = 0;
            writeAccelerationStructure.dstArrayElement = 0;
            writeAccelerationStructure.descriptorCount = 1;
            writeAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            writes.push_back(writeAccelerationStructure);

            VkWriteDescriptorSet writeBindlessAccelerationStructure{};
            writeBindlessAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeBindlessAccelerationStructure.pNext = &descriptorAccelerationStructure;
            writeBindlessAccelerationStructure.dstSet = GraphicsPipelineManager::GetBindlessDescriptorSet();
            writeBindlessAccelerationStructure.dstBinding = GraphicsPipelineManager::ACCELERATION_STRUCTURE_BINDING;
            writeBindlessAccelerationStructure.dstArrayElement = 0;
            writeBindlessAccelerationStructure.descriptorCount = 1;
            writeBindlessAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            writes.push_back(writeBindlessAccelerationStructure);

            vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
        }

        BufferDesc scratchDesc;
        scratchDesc.size = sizeInfo.buildScratchSize;
        scratchDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        scratchDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        BufferManager::Create(scratchDesc, scratchBuffer);

        VkBufferDeviceAddressInfo scratchInfo{};
        scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchInfo.buffer = scratchBuffer.buffer;
        VkDeviceAddress scratchAddress = ctx.vkGetBufferDeviceAddressKHR(device, &scratchInfo);

        // Update build information
        buildInfo.srcAccelerationStructure  = ctx.TLAS.accel;
        buildInfo.dstAccelerationStructure  = ctx.TLAS.accel;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        // Build Offsets info: n instances
        VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{countInstance, 0, 0, 0};
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

        // Build the TLAS
        ctx.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildOffsetInfo);
    }

    LogicalDevice::EndSingleTimeCommands(commandBuffer);
    BufferManager::Destroy(scratchBuffer);
    BufferManager::Destroy(instancesBuffer);
}

void CreateImage() {
    LUZ_PROFILE_FUNC();
    ImageDesc imageDesc;
    imageDesc.width = SwapChain::GetExtent().width;
    imageDesc.height = SwapChain::GetExtent().height;
    imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageDesc.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageDesc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imageDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    imageDesc.format = SwapChain::GetImageFormat();
    imageDesc.mipLevels = 1;
    imageDesc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    imageDesc.layout = VK_IMAGE_LAYOUT_GENERAL;
    ImageManager::Create(imageDesc, ctx.image);
    ctx.sampler = CreateSampler(1);
    ctx.imguiTextureID = ImGui_ImplVulkan_AddTexture(ctx.sampler, ctx.image.view, VK_IMAGE_LAYOUT_GENERAL);

    ctx.viewport.x = 0;
    ctx.viewport.y = 0;
    ctx.viewport.width = imageDesc.width;
    ctx.viewport.height = imageDesc.height;
    ctx.viewport.minDepth = 0;
    ctx.viewport.maxDepth = 1;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = ctx.image.view;
    imageInfo.sampler = VK_NULL_HANDLE;

    std::vector<VkWriteDescriptorSet> writes;

    VkWriteDescriptorSet writeImage{};
    writeImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeImage.dstSet = ctx.descriptorSet;
    writeImage.dstBinding = 1;
    writeImage.dstArrayElement = 0;
    writeImage.descriptorCount = 1;
    writeImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeImage.pImageInfo = &imageInfo;
    writes.push_back(writeImage);

    vkUpdateDescriptorSets(LogicalDevice::GetVkDevice(), writes.size(), writes.data(), 0, nullptr);
}

void CreatePipeline() {
    LUZ_PROFILE_FUNC();
    VkDevice device = LogicalDevice::GetVkDevice();

    enum StageIndices {
        RaygenStage,
        MissStage,
        MissStage2,
        ClosestHitStage,
        ShaderGroupCount
    };

    std::vector<VkPipelineShaderStageCreateInfo> stages(ShaderGroupCount);

    ShaderDesc raygenDesc;
    raygenDesc.shaderBytes = FileManager::ReadRawBytes("bin/rt.rgen.spv");
    raygenDesc.stageBit = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    ShaderResource raygenStage;
    Shader::Create(raygenDesc, raygenStage);
    stages[RaygenStage] = raygenStage.stageCreateInfo;

    ShaderDesc missDesc;
    missDesc.shaderBytes = FileManager::ReadRawBytes("bin/rt.rmiss.spv");
    missDesc.stageBit = VK_SHADER_STAGE_MISS_BIT_KHR;
    ShaderResource missStage;
    Shader::Create(missDesc, missStage);
    stages[MissStage] = missStage.stageCreateInfo;

    ShaderDesc shadowDesc;
    shadowDesc.shaderBytes = FileManager::ReadRawBytes("bin/rtShadow.rmiss.spv");
    shadowDesc.stageBit = VK_SHADER_STAGE_MISS_BIT_KHR;
    ShaderResource shadowStage;
    Shader::Create(shadowDesc, shadowStage);
    stages[MissStage2] = shadowStage.stageCreateInfo;

    ShaderDesc hitDesc;
    hitDesc.shaderBytes = FileManager::ReadRawBytes("bin/rt.rchit.spv");
    hitDesc.stageBit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    ShaderResource hitStage;
    Shader::Create(hitDesc, hitStage);
    stages[ClosestHitStage] = hitStage.stageCreateInfo;

    VkRayTracingShaderGroupCreateInfoKHR group{};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.anyHitShader       = VK_SHADER_UNUSED_KHR;
    group.closestHitShader   = VK_SHADER_UNUSED_KHR;
    group.generalShader      = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = RaygenStage;
    ctx.shaderGroups.push_back(group);

    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = MissStage;
    ctx.shaderGroups.push_back(group);

    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = MissStage2;
    ctx.shaderGroups.push_back(group);

    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = ClosestHitStage;
    ctx.shaderGroups.push_back(group);

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(RayTracingPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    // descriptor sets
    {
        VkDescriptorPoolSize poolSizes[] = { 
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 3}, 
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3} 
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = (uint32_t)(3);
        poolInfo.poolSizeCount = sizeof(poolSizes)/sizeof(VkDescriptorPoolSize);
        poolInfo.pPoolSizes = poolSizes;

        VkResult res = vkCreateDescriptorPool(device, &poolInfo, nullptr, &ctx.descriptorPool);
        DEBUG_VK(res, "Failed to create ray tracing descriptor pool!");

        VkDescriptorSetLayoutBinding bindings[2];
        VkDescriptorBindingFlags bindingFlags[2];
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[0].pImmutableSamplers = VK_NULL_HANDLE;
        bindingFlags[0] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[1].pImmutableSamplers = VK_NULL_HANDLE;
        bindingFlags[1] = 0;

        VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
        setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        setLayoutBindingFlags.bindingCount = COUNT_OF(bindingFlags);
        setLayoutBindingFlags.pBindingFlags = bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = sizeof(bindings)/sizeof(VkDescriptorSetLayoutBinding);
        layoutInfo.pBindings = bindings;
        layoutInfo.pNext = &setLayoutBindingFlags;
        res = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &ctx.descriptorSetLayout);
        DEBUG_VK(res, "Failed to create ray tracing descriptor set layout!");

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = ctx.descriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &ctx.descriptorSetLayout;
        res = vkAllocateDescriptorSets(device, &allocateInfo, &ctx.descriptorSet);
        DEBUG_VK(res, "Failed to allocate ray tracing descriptor set!");
    }

    // image
    {
        CreateImage();
    }

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &ctx.descriptorSetLayout;

    VkResult res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &ctx.pipelineLayout);
    DEBUG_VK(res, "Failed to create ray tracing pipeline layout!");

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pDynamicStates = dynamicStates;
    dynamicInfo.dynamicStateCount = COUNT_OF(dynamicStates);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = (u32)stages.size();
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = (u32)ctx.shaderGroups.size();
    pipelineInfo.pGroups = ctx.shaderGroups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = ctx.pipelineLayout;
    pipelineInfo.pDynamicState = &dynamicInfo;
        
    ctx.vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, & pipelineInfo, nullptr, & ctx.pipeline);

    for (VkPipelineShaderStageCreateInfo& stage : stages) {
        vkDestroyShaderModule(device, stage.module, nullptr);
    }
}

void CreateShaderBindingTable() {
    VkDevice device = LogicalDevice::GetVkDevice();

    ctx.properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 prop2{};
    prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    prop2.pNext = &ctx.properties;
    vkGetPhysicalDeviceProperties2(PhysicalDevice::GetVkPhysicalDevice(), &prop2);

    u32 missCount = 2;
    u32 hitCount = 1;
    u32 handleCount = 1 + missCount + hitCount;
    u32 handleSize= ctx.properties.shaderGroupHandleSize;
    u32 handleSizeAligned = ALIGN_AS(handleSize, ctx.properties.shaderGroupHandleAlignment);

    ctx.rgenRegion.stride = ALIGN_AS(handleSizeAligned, ctx.properties.shaderGroupBaseAlignment);
    ctx.rgenRegion.size = ctx.rgenRegion.stride;

    ctx.missRegion.stride = handleSizeAligned;
    ctx.missRegion.size = ALIGN_AS(missCount * handleSizeAligned, ctx.properties.shaderGroupBaseAlignment);

    ctx.hitRegion.stride = handleSizeAligned;
    ctx.hitRegion.size = ALIGN_AS(hitCount * handleSizeAligned, ctx.properties.shaderGroupBaseAlignment);

    u32 dataSize = handleCount * handleSize;
    std::vector<u8> handles(dataSize);
    VkResult res = ctx.vkGetRayTracingShaderGroupHandlesKHR(device, ctx.pipeline, 0, handleCount, dataSize, handles.data());
    DEBUG_VK(res, "Failed to get ray tracing shader group handles!");

    BufferDesc sbtDesc;
    sbtDesc.size = ctx.rgenRegion.size + ctx.missRegion.size + ctx.hitRegion.size + ctx.callRegion.size;
    sbtDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    sbtDesc.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    BufferManager::Create(sbtDesc, ctx.SBTBuffer);

    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = ctx.SBTBuffer.buffer;
    VkDeviceAddress sbtAddress = ctx.vkGetBufferDeviceAddressKHR(device, &info);

    ctx.rgenRegion.deviceAddress = sbtAddress;
    ctx.missRegion.deviceAddress = sbtAddress + ctx.rgenRegion.size;
    ctx.hitRegion.deviceAddress = sbtAddress + ctx.rgenRegion.size + ctx.missRegion.size;

    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };
    void* pvoidSBTBuffer;
    u8* pData = nullptr;
    u32 handleIdx = 0;

    vkMapMemory(device, ctx.SBTBuffer.memory, 0, sbtDesc.size, 0, &pvoidSBTBuffer);
    u8* pSBTBuffer = (u8*)pvoidSBTBuffer;

    // ray gen
    pData = pSBTBuffer;
    memcpy(pData, getHandle(handleIdx++), handleSize);

    // miss
    pData = pSBTBuffer + ctx.rgenRegion.size;
    for (u32 c = 0; c < missCount; c++) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += ctx.missRegion.stride;
    }

    // hit
    pData = pSBTBuffer + ctx.rgenRegion.size + ctx.missRegion.size;
    for (u32 c = 0; c < hitCount; c++) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += ctx.hitRegion.stride;
    }

    vkUnmapMemory(device, ctx.SBTBuffer.memory);
}

void Create() {
    LUZ_PROFILE_FUNC();
    VkDevice device = LogicalDevice::GetVkDevice();
    ctx.vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    ctx.vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    ctx.vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
    ctx.vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    ctx.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    ctx.vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    ctx.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    ctx.vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
    ctx.vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    // CreateBLAS();
    // CreateTLAS(Scene::modelEntities);
    CreatePipeline();
    CreateShaderBindingTable();
}

void Destroy() {
    VkDevice device = LogicalDevice::GetVkDevice();
    for (int i = 0; i < ctx.BLAS.size(); i++) {
        ctx.vkDestroyAccelerationStructureKHR(device, ctx.BLAS[i].accel, nullptr);
        BufferManager::Destroy(ctx.BLAS[i].buffer);
    }
    ctx.BLAS.clear();
    ctx.vkDestroyAccelerationStructureKHR(device, ctx.TLAS.accel, nullptr);
    BufferManager::Destroy(ctx.TLAS.buffer);
    ImageManager::Destroy(ctx.image);
    BufferManager::Destroy(ctx.SBTBuffer);
    vkDestroyDescriptorPool(device, ctx.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, ctx.descriptorSetLayout, nullptr);
    vkDestroySampler(device, ctx.sampler, nullptr);
    vkDestroyPipelineLayout(device, ctx.pipelineLayout, nullptr);
    vkDestroyPipeline(device, ctx.pipeline, nullptr);
    ctx.TLAS.accel = VK_NULL_HANDLE;
}

void RayTrace(VkCommandBuffer& commandBuffer) {
    LUZ_PROFILE_FUNC();
    if (ctx.useRayTracing) {
        RayTracingPushConstants constants;
        // constants.clearColor = glm::vec4(0, 0, 1, 1);
        // constants.lightPosition = glm::vec3(20, 0, 0);
        // constants.lightIntensity = 3.0f;
        // constants.viewProj = Scene::camera.GetView() * Scene::camera.GetProj();
        // constants.viewInverse = glm::inverse(Scene::camera.GetView());
        // constants.projInverse = glm::inverse(Scene::camera.GetProj());
        constants.view = Scene::camera.GetView();
        constants.proj = Scene::camera.GetProj();

        auto ext = SwapChain::GetExtent();

        VkPipelineBindPoint point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        vkCmdBindPipeline(commandBuffer, point, ctx.pipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &ctx.viewport);
        vkCmdBindDescriptorSets(commandBuffer, point, ctx.pipelineLayout, 0, 1, &ctx.descriptorSet, 0, nullptr);
        auto stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
        vkCmdPushConstants(commandBuffer, ctx.pipelineLayout, stages, 0, sizeof(RayTracingPushConstants), &constants);
        ctx.vkCmdTraceRaysKHR(commandBuffer, &ctx.rgenRegion, &ctx.missRegion, &ctx.hitRegion, &ctx.callRegion, ext.width, ext.height, 1);
    }
}

void OnImgui() {
    LUZ_PROFILE_FUNC();
    if (ImGui::Begin("Ray Tracing")) {
        ImGui::Checkbox("Enabled", &ctx.useRayTracing);
        ImVec2 imgSize = ImVec2(ctx.image.width, ctx.image.height);
        ImVec2 winSize = ImGui::GetContentRegionAvail();
        float t;
        if (imgSize.x / imgSize.y > winSize.x / winSize.y) {
            t = winSize.x / imgSize.x;
        } else {
            t = winSize.y / imgSize.y;
        }
        ImVec2 size = ImVec2(imgSize.x * t, imgSize.y*t);
        ImGui::Image(ctx.imguiTextureID, size);
    }
    ImGui::End();
}

void UpdateViewport(VkExtent2D ext) {
    ImageManager::Destroy(ctx.image);
    vkDestroySampler(LogicalDevice::GetVkDevice(), ctx.sampler, nullptr);
    CreateImage();
}

void SetRecreateTLAS() {
    ctx.recreateTLAS = true;
}

}
