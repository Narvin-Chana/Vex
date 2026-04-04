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
    ::vk::TransformMatrixKHR mat;
    for (int i = 0; i < matrix.size(); ++i)
    {
        mat.matrix[i / 4][i % 4] = matrix[i];
    }
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

    auto bindingToOffsetCount = [](const std::optional<RHIBufferBinding>& binding,
                                   u32 fallbackStride) -> std::pair<u32, u32> // offset, count
    {
        if (!binding)
            return {};

        return { static_cast<u32>(binding->binding.offsetByteSize.value_or(0) / fallbackStride),
                 static_cast<u32>(binding->binding.rangeByteSize.value_or(binding->buffer->GetDesc().byteSize) /
                                  fallbackStride) };
    };

    for (const RHIBLASGeometryDesc& geom : desc.geometries)
    {
        ::vk::AccelerationStructureGeometryKHR geometry{};
        if (desc.type == ASGeometryType::Triangles)
        {
            VEX_ASSERT(geom.vertexBufferBinding);
            VEX_ASSERT(geom.vertexBufferBinding->binding.strideByteSize);

            static constexpr u32 vertexByteStride = sizeof(float) * 3;

            auto [firstVertex, vertexCount] = bindingToOffsetCount(geom.vertexBufferBinding, vertexByteStride);

            u32 triangleCount = vertexCount / 3;
            if (geom.indexBufferBinding)
            {
                triangleCount = bindingToOffsetCount(geom.indexBufferBinding, sizeof(u32)).second;
            }

            geometryCount.push_back(triangleCount);

            BufferBinding vertexBufferBinding = geom.vertexBufferBinding->binding;

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eTriangles,
                .geometry = {
                    .triangles = {
                        .vertexFormat = ::vk::Format::eR32G32B32Sfloat,
                        .vertexData = { geom.vertexBufferBinding->buffer->GetDeviceAddress() },
                        .vertexStride = sizeof(float) * 3,
                        .maxVertex = vertexCount - 1,
                        .indexType = geom.indexBufferBinding ? ::vk::IndexType::eUint32 : ::vk::IndexType::eNoneKHR,
                        .indexData = { geom.indexBufferBinding ? geom.indexBufferBinding->buffer->GetDeviceAddress() : ::vk::DeviceAddress{} },
                        .transformData = { geom.transformBufferBinding ? geom.transformBufferBinding->buffer->GetDeviceAddress() : ::vk::DeviceAddress{} },
                    },
                },
            };

            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = triangleCount,
                .primitiveOffset = static_cast<u32>(geom.indexBufferBinding
                                                        ? geom.indexBufferBinding->binding.offsetByteSize.value_or(0)
                                                        : firstVertex * vertexByteStride),
                .firstVertex = 0,
                .transformOffset =
                    geom.transformBufferBinding
                        ? static_cast<u32>(geom.transformBufferBinding->binding.offsetByteSize.value_or(0))
                        : 0,
            });
        }
        else if (desc.type == ASGeometryType::AABBs)
        {
            VEX_ASSERT(geom.aabbBufferBinding);

            geometry = ::vk::AccelerationStructureGeometryKHR{
                .geometryType = ::vk::GeometryTypeKHR::eAabbs,
                .geometry = {
                    .aabbs = {
                        .data = { geom.aabbBufferBinding->buffer->GetDeviceAddress() },
                        .stride = sizeof(VkAabbPositionsKHR),
                    },
                },
            };

            auto [firstAabb, aabbCount] = bindingToOffsetCount(geom.aabbBufferBinding, sizeof(float) * 6);

            geometryCount.push_back(aabbCount);
            ranges.push_back(::vk::AccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = aabbCount,
                .primitiveOffset = static_cast<u32>(geom.aabbBufferBinding->binding.offsetByteSize.value_or(0)),
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
                .data = { desc.instancesBinding->buffer->GetDeviceAddress() }
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
        .scratchByteSize = AlignUp(static_cast<u32>(asBuildSize.buildScratchSize), minASscratchAlignment),
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