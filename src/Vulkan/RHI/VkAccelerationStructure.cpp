#include "VkAccelerationStructure.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/Utility/ByteUtils.h>

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

namespace
{
::vk::TransformMatrixKHR GetVkTransformMatrix(std::array<float, 3 * 4> matrix)
{
    // TODO: make this a loop
    ::vk::TransformMatrixKHR mat;
    mat.matrix[0][0] = matrix[0];
    mat.matrix[0][1] = matrix[1];
    mat.matrix[0][2] = matrix[2];
    mat.matrix[0][3] = matrix[3];

    mat.matrix[1][0] = matrix[4];
    mat.matrix[1][1] = matrix[5];
    mat.matrix[1][2] = matrix[6];
    mat.matrix[1][3] = matrix[7];

    mat.matrix[2][0] = matrix[8];
    mat.matrix[2][1] = matrix[9];
    mat.matrix[2][2] = matrix[10];
    mat.matrix[2][3] = matrix[11];
    return mat;
}
} // namespace

::vk::GeometryFlagsKHR GeometryFlagsToVkGeometryFlags(ASGeometry::Flags flags)
{
    ::vk::GeometryFlagsKHR vkFlags{};
    if (flags & ASGeometry::Opaque)
        vkFlags |= ::vk::GeometryFlagBitsKHR::eOpaque;
    if (flags & ASGeometry::NoDuplicateAnyHitInvocation)
        vkFlags |= ::vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
    return vkFlags;
}

::vk::GeometryInstanceFlagsKHR ASInstanceFlagsToVkGeometryInstanceFlags(ASInstance::Flags flags)
{
    ::vk::GeometryInstanceFlagsKHR vkFlags{};
    if (flags & ASInstance::ForceNonOpaque)
        vkFlags |= ::vk::GeometryInstanceFlagBitsKHR::eForceNoOpaque;
    if (flags & ASInstance::ForceOpaque)
        vkFlags |= ::vk::GeometryInstanceFlagBitsKHR::eForceOpaque;
    if (flags & ASInstance::TriangleCullDisable)
        vkFlags |= ::vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable;
    if (flags & ASInstance::TriangleFrontCounterClockwise)
        vkFlags |= ::vk::GeometryInstanceFlagBitsKHR::eTriangleFrontCounterclockwise;
    return vkFlags;
}

::vk::BuildAccelerationStructureFlagsKHR ASBuildFlagsToVkASBuildFlags(ASBuild::Flags flags)
{
    using enum ::vk::BuildAccelerationStructureFlagBitsKHR;
    ::vk::BuildAccelerationStructureFlagsKHR vkFlags{};
    if (flags & ASBuild::AllowCompaction)
        vkFlags |= eAllowCompaction;
    if (flags & ASBuild::AllowUpdate)
        vkFlags |= eAllowUpdate;
    if (flags & ASBuild::MinimizeMemory)
        vkFlags |= eLowMemory;
    if (flags & ASBuild::PreferFastBuild)
        vkFlags |= ePreferFastBuild;
    if (flags & ASBuild::PreferFastTrace)
        vkFlags |= ePreferFastTrace;
    return vkFlags;
}

VkAccelerationStructure::VkAccelerationStructure(NonNullPtr<VkGPUContext> ctx, const ASDesc& desc)
    : RHIAccelerationStructureBase(desc)
    , ctx{ ctx }
{
}

const RHIAccelerationStructureBuildInfo& VkAccelerationStructure::SetupBLASBuild(RHIAllocator& allocator,
                                                                                 const RHIBLASBuildDesc& desc)
{
    geometries.clear();
    ranges.clear();
    geometryCount.clear();

    geometries.reserve(desc.geometries.size());
    ranges.reserve(desc.geometries.size());
    geometryCount.reserve(desc.geometries.size());

    for (const RHIBLASGeometryDesc& geom : desc.geometries)
    {
        ::vk::AccelerationStructureGeometryKHR geometry{};
        if (desc.type == ASGeometryType::Triangles)
        {
            VEX_ASSERT(geom.vertexBufferBinding);
            VEX_ASSERT(geom.vertexBufferBinding->binding.strideByteSize);
            VEX_ASSERT(geom.indexBufferBinding);

            auto indexCount = *geom.indexBufferBinding->binding.rangeByteSize / sizeof(u32);
            u32 triangleCount = indexCount / 3;
            geometryCount.push_back(triangleCount);

            BufferBinding vertexBufferBinding = geom.vertexBufferBinding->binding;

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eTriangles,
                .geometry = {
                    .triangles = {
                        .vertexFormat = ::vk::Format::eR32G32B32Sfloat,
                        .vertexData = { geom.vertexBufferBinding->buffer->GetDeviceAddress() },
                        .vertexStride = *vertexBufferBinding.strideByteSize,
                        .maxVertex = static_cast<u32>(*vertexBufferBinding.rangeByteSize / static_cast<u64>(*vertexBufferBinding.strideByteSize)) - 1,
                        .indexType = ::vk::IndexType::eUint32,
                        .indexData = { geom.indexBufferBinding->buffer->GetDeviceAddress() },
                        .transformData = { geom.transformBufferBinding ? geom.transformBufferBinding->buffer->GetDeviceAddress() : ::vk::DeviceAddress{} },
                    },
                },
            };

            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = triangleCount,
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0,
            });
        }
        else if (desc.type == ASGeometryType::AABBs)
        {
            VEX_ASSERT(geom.vertexBufferBinding);

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eAabbs,
                .geometry = {
                    .aabbs = {
                        .data = { geom.aabbBufferBinding->buffer->GetDeviceAddress() },
                        .stride = sizeof(VkAabbPositionsKHR),
                    },
                },
            };

            geometryCount.push_back(1);
            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = 1,
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0,
            });
        }

        geometry.flags = GeometryFlagsToVkGeometryFlags(geom.flags);

        geometries.push_back(geometry);
    }

    BuildAccelerationStructure(::vk::AccelerationStructureTypeKHR::eBottomLevel, allocator);

    return prebuildInfo;
}

const RHIAccelerationStructureBuildInfo& VkAccelerationStructure::SetupTLASBuild(RHIAllocator& allocator,
                                                                                 const RHITLASBuildDesc& desc)
{
    geometries.clear();
    ranges.clear();
    geometryCount.clear();

    VEX_ASSERT(desc.instancesBinding);

    geometries.push_back(::vk::AccelerationStructureGeometryKHR{
        .geometryType = ::vk::GeometryTypeKHR::eInstances,
        .geometry = {
            .instances = {
                .arrayOfPointers = false,
                .data = { desc.instancesBinding->buffer->GetDeviceAddress() }
            },
        },
    });
    geometryCount.push_back(1);
    ranges.push_back({
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    });

    BuildAccelerationStructure(::vk::AccelerationStructureTypeKHR::eTopLevel, allocator);

    return prebuildInfo;
}

std::vector<std::byte> VkAccelerationStructure::GetInstanceBufferData(const RHITLASBuildDesc& desc)
{
    std::vector<::vk::AccelerationStructureInstanceKHR> instances;
    for (u32 i = 0; i < desc.instances.size(); i++)
    {
        const TLASInstanceDesc& instance = desc.instances[i];
        const NonNullPtr<RHIAccelerationStructure> as = desc.perInstanceBLAS[i];

        instances.push_back(::vk::AccelerationStructureInstanceKHR{
            .transform = GetVkTransformMatrix(instance.transform),
            .instanceCustomIndex = instance.instanceID,
            .mask = instance.instanceMask,
            .instanceShaderBindingTableRecordOffset = instance.instanceContributionToHitGroupIndex,
            .flags = static_cast<VkGeometryInstanceFlagsKHR>(
                ASInstanceFlagsToVkGeometryInstanceFlags(instance.instanceFlags)),
            .accelerationStructureReference = as->GetNativeAddress(),
        });
    }

    std::span byteSpan{ reinterpret_cast<std::byte*>(instances.data()),
                        instances.size() * sizeof(::vk::AccelerationStructureInstanceKHR) };
    return { byteSpan.begin(), byteSpan.end() };
}

u32 VkAccelerationStructure::GetInstanceBufferStride()
{
    return sizeof(::vk::AccelerationStructureInstanceKHR);
}

::vk::DeviceAddress VkAccelerationStructure::GetNativeAddress()
{
    return VEX_VK_CHECK <<= ctx->device.getAccelerationStructureAddressKHR({
               .accelerationStructure = *vkAccelerationStructure,
           });
}

void VkAccelerationStructure::BuildAccelerationStructure(::vk::AccelerationStructureTypeKHR type,
                                                         RHIAllocator& allocator)
{
    ::vk::AccelerationStructureBuildGeometryInfoKHR asBuildInfo{
        .type = type,
        .flags = ASBuildFlagsToVkASBuildFlags(GetDesc().buildFlags),
        .mode = ::vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = static_cast<u32>(geometries.size()),
        .pGeometries = geometries.data(), // The geometry to build the acceleration structure from
    };

    // TODO: Move this to VkPhysicalDevice
    ::vk::StructureChain<::vk::PhysicalDeviceProperties2, ::vk::PhysicalDeviceAccelerationStructurePropertiesKHR>
        propertiesChain = ctx->physDevice.getProperties2<::vk::PhysicalDeviceProperties2,
                                                         ::vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
    auto minASscratchAlignment = propertiesChain.get<::vk::PhysicalDeviceAccelerationStructurePropertiesKHR>()
                                     .minAccelerationStructureScratchOffsetAlignment;

    ::vk::AccelerationStructureBuildSizesInfoKHR asBuildSize =
        ctx->device.getAccelerationStructureBuildSizesKHR(::vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                          asBuildInfo,
                                                          geometryCount);

    prebuildInfo = {
        .asByteSize = asBuildSize.accelerationStructureSize,
        .scratchByteSize = AlignUp(static_cast<u32>(asBuildSize.buildScratchSize), minASscratchAlignment),
        .updateScratchByteSize = asBuildSize.updateScratchSize,
    };

    BufferDesc asBufferDesc{
        .name = GetDesc().name,
        .byteSize = prebuildInfo.asByteSize,
        .usage = BufferUsage::AccelerationStructure | BufferUsage::ReadWriteBuffer,
        .memoryLocality = ResourceMemoryLocality::GPUOnly,
    };
    accelerationStructure = RHIBuffer(ctx, allocator, asBufferDesc);

    ::vk::AccelerationStructureCreateInfoKHR asCreateInfo{
        .buffer = accelerationStructure->GetNativeBuffer(),
        .size = asBuildSize.accelerationStructureSize,
        .type = type,
    };
    vkAccelerationStructure = VEX_VK_CHECK <<= ctx->device.createAccelerationStructureKHRUnique(asCreateInfo);
}

} // namespace vex::vk