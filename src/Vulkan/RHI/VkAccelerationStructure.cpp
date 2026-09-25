#include "VkAccelerationStructure.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/Utility/ByteUtils.h>

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

namespace VkAccelerationStructure_Internal
{
::vk::TransformMatrixKHR GetVkTransformMatrix(std::array<float, 3 * 4> matrix)
{
    ::vk::TransformMatrixKHR mat;
    std::copy_n(matrix.data(), 12, &mat.matrix[0][0]);
    return mat;
}
} // namespace VkAccelerationStructure_Internal

::vk::GeometryFlagsKHR GeometryFlagsToVkGeometryFlags(Flags<ASGeometry> flags)
{
    ::vk::GeometryFlagsKHR vkFlags{};
    if (flags & ASGeometry::Opaque)
        vkFlags |= ::vk::GeometryFlagBitsKHR::eOpaque;
    if (flags & ASGeometry::NoDuplicateAnyHitInvocation)
        vkFlags |= ::vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
    return vkFlags;
}

::vk::GeometryInstanceFlagsKHR ASInstanceFlagsToVkGeometryInstanceFlags(Flags<ASInstance> flags)
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

::vk::BuildAccelerationStructureFlagsKHR ASBuildFlagsToVkASBuildFlags(Flags<ASBuild> flags)
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

VkAccelerationStructure::VkAccelerationStructure(NonNullPtr<VkGPUContext> ctx, const AccelerationStructureDesc& desc)
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
            VEX_ASSERT(geom.vertexBufferView);
            VEX_ASSERT(geom.vertexBufferView->view.strideByteSize);

            // ?? FirstVertex is unused after here?
            // There's a firstVertex field in AccelerationStructureBuildRangeInfoKHR, maybe there?
            const u32 vertexCount =
                static_cast<u32>(geom.vertexBufferView->view.GetElementCount(geom.vertexBufferView->buffer->GetDesc()));

            ::vk::IndexType indexType = ::vk::IndexType::eNoneKHR;
            u32 triangleCount = vertexCount / 3;
            if (geom.indexBufferView)
            {
                switch (geom.indexBufferView->format)
                {
                case IndexFormat::U16:
                    indexType = ::vk::IndexType::eUint16;
                    break;
                case IndexFormat::U32:
                    indexType = ::vk::IndexType::eUint32;
                    break;
                default:
                    VEX_ASSERT(false, "Unsupported index format");
                }
                const u32 indexCount =
                    geom.indexBufferView->region.GetByteSize(geom.indexBufferView->buffer->GetDesc()) /
                    (std::to_underlying(geom.indexBufferView->format) / 8);
                triangleCount = indexCount / 3;
            }

            geometryCount.push_back(triangleCount);

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eTriangles,
                .geometry = {
                    .triangles = {
                        .vertexFormat = ::vk::Format::eR32G32B32Sfloat,
                        .vertexData = { geom.vertexBufferView->buffer->GetDeviceAddress() + geom.vertexBufferView->view.region.byteOffset },
                        .vertexStride = geom.vertexBufferView->view.strideByteSize,
                        .maxVertex = vertexCount - 1,
                        .indexType = indexType,
                        .indexData = { geom.indexBufferView ? geom.indexBufferView->buffer->GetDeviceAddress() + geom.indexBufferView->region.byteOffset : ::vk::DeviceAddress{} },
                        .transformData = { geom.transformBufferView ? geom.transformBufferView->buffer->GetDeviceAddress() + geom.transformBufferView->view.region.byteOffset : ::vk::DeviceAddress{} },
                    },
                },
            };

            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = triangleCount,
                .primitiveOffset = static_cast<u32>(geom.indexBufferView ? geom.indexBufferView->region.byteOffset : 0),
                .firstVertex = 0,
                .transformOffset =
                    geom.transformBufferView ? static_cast<u32>(geom.transformBufferView->view.region.byteOffset) : 0,
            });
        }
        else if (desc.type == ASGeometryType::AABBs)
        {
            VEX_ASSERT(geom.aabbBufferView);

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eAabbs,
                .geometry = {
                    .aabbs = {
                        .data = { geom.aabbBufferView->buffer->GetDeviceAddress() },
                        .stride = sizeof(VkAabbPositionsKHR),
                    },
                },
            };

            const u32 aabbCount = geom.aabbBufferView->view.GetElementCount(geom.aabbBufferView->buffer->GetDesc());
            geometryCount.push_back(aabbCount);
            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = aabbCount,
                .primitiveOffset = static_cast<u32>(geom.aabbBufferView->view.region.byteOffset),
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

    VEX_ASSERT(desc.instancesView);

    geometries.push_back(::vk::AccelerationStructureGeometryKHR{
        .geometryType = ::vk::GeometryTypeKHR::eInstances,
        .geometry = {
            .instances = {
                .data = { desc.instancesView->buffer->GetDeviceAddress() }
            },
        },
    });

    auto primitiveCount = static_cast<u32>(desc.instances.size());
    ranges.push_back({
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
    });
    geometryCount.push_back(primitiveCount);

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
            .transform = VkAccelerationStructure_Internal::GetVkTransformMatrix(instance.transform),
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

    // TODO(https://trello.com/c/ZUZPpce4): Move this to VkPhysicalDevice class
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
        .scratchByteSize = ByteUtil::AlignUp(static_cast<u32>(asBuildSize.buildScratchSize), minASscratchAlignment),
        .updateScratchByteSize = asBuildSize.updateScratchSize,
    };

    BufferDesc asBufferDesc{
        .name = GetDesc().name,
        .byteSize = prebuildInfo.asByteSize,
        .usage = BufferUsage::AccelerationStructure,
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