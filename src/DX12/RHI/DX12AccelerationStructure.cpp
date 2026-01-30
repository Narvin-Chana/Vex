#include "DX12AccelerationStructure.h"

#include <Vex/Utility/ByteUtils.h>

#include <DX12/RHI/DX12Buffer.h>

namespace vex::dx12
{

DX12AccelerationStructure::DX12AccelerationStructure(ComPtr<DX12Device>& device, const ASDesc& desc)
    : RHIAccelerationStructureBase(desc)
    , device(device)
{
}

const RHIAccelerationStructureBuildInfo& DX12AccelerationStructure::SetupBLASBuild(RHIAllocator& allocator,
                                                                                   const RHIBLASBuildDesc& desc)
{
    VEX_ASSERT(!accelerationStructure.has_value(),
               "Cannot call setup when the acceleration structure is already setup!");
    InitRayTracingGeometryDesc(desc);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags = ASBuildFlagsToDX12ASBuildFlags(GetDesc().buildFlags),
        .NumDescs = static_cast<u32>(geometryDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT::D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = geometryDescs.data(),
        // TODO(https://trello.com/c/YPn5ypzR): handle opacity micromaps
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO dx12PrebuildInfo{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &dx12PrebuildInfo);

    prebuildInfo = {
        .asByteSize = dx12PrebuildInfo.ResultDataMaxSizeInBytes,
        .scratchByteSize = dx12PrebuildInfo.ScratchDataSizeInBytes,
        .updateScratchByteSize = dx12PrebuildInfo.UpdateScratchDataSizeInBytes,
    };

    // Create the actual AS buffer.
    BufferDesc asDesc{
        .name = GetDesc().name,
        .byteSize = prebuildInfo.asByteSize,
        .usage = BufferUsage::AccelerationStructure | BufferUsage::ReadWriteBuffer,
        .memoryLocality = ResourceMemoryLocality::GPUOnly,
    };
    accelerationStructure = RHIBuffer(device, allocator, asDesc);

    return prebuildInfo;
}

const RHIAccelerationStructureBuildInfo& DX12AccelerationStructure::SetupTLASBuild(RHIAllocator& allocator,
                                                                                   const RHITLASBuildDesc& desc)
{
    VEX_ASSERT(!accelerationStructure.has_value(),
               "Cannot call setup when the acceleration structure is already setup!");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = ASBuildFlagsToDX12ASBuildFlags(GetDesc().buildFlags),
        .NumDescs = static_cast<u32>(desc.instanceDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO dx12PrebuildInfo{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &dx12PrebuildInfo);

    prebuildInfo = {
        .asByteSize = dx12PrebuildInfo.ResultDataMaxSizeInBytes,
        .scratchByteSize = dx12PrebuildInfo.ScratchDataSizeInBytes,
        .updateScratchByteSize = dx12PrebuildInfo.UpdateScratchDataSizeInBytes,
        .uploadBufferByteSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * desc.instanceDescs.size(),
    };

    // Create the actual AS buffer.
    BufferDesc asDesc{
        .name = GetDesc().name,
        .byteSize = prebuildInfo.asByteSize,
        .usage = BufferUsage::AccelerationStructure | BufferUsage::ReadWriteBuffer,
        .memoryLocality = ResourceMemoryLocality::GPUOnly,
    };
    accelerationStructure = RHIBuffer(device, allocator, asDesc);

    return prebuildInfo;
}

void DX12AccelerationStructure::InitRayTracingGeometryDesc(const RHIBLASBuildDesc& desc)
{
    // Fill in the BLAS geometry descs.
    geometryDescs.clear();
    geometryDescs.reserve(desc.geometry.size());
    for (const RHIBLASGeometryDesc& rhiGeometryDesc : desc.geometry)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
        geometryDesc.Flags = ASGeometryFlagsToDX12GeometryFlags(rhiGeometryDesc.flags);

        if (desc.type == ASGeometryType::Triangles)
        {
            geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

            const D3D12_VERTEX_BUFFER_VIEW vbView = rhiGeometryDesc.vertexBufferBinding->buffer->GetVertexBufferView(
                rhiGeometryDesc.vertexBufferBinding->binding);
            geometryDesc.Triangles.VertexBuffer = {
                .StartAddress = vbView.BufferLocation,
                .StrideInBytes = vbView.StrideInBytes,
            };
            geometryDesc.Triangles.VertexCount = vbView.SizeInBytes / vbView.StrideInBytes;
            geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            // TODO(https://trello.com/c/srGndUSP): Handle other vertex formats, this should be cross-referenced
            // with Vulkan to make sure only formats supported by both APIs are accepted.
            if (vbView.StrideInBytes > sizeof(float) * 3)
            {
                VEX_LOG(
                    Warning,
                    "Vex currently does not support acceleration structure geometry whose vertices have a format "
                    "different to 12 bytes (RGB32). Your vertex buffer binding has a different stride than this, this "
                    "is ok as long as the user is aware that elements outside the first 12 bytes will be ignored.");
            }

            VEX_ASSERT(vbView.StrideInBytes < sizeof(float) * 3,
                       "Vex currently does not support acceleration structure geometry whose vertices have a stride "
                       "smaller than 12 bytes.");

            if (rhiGeometryDesc.indexBufferBinding.has_value())
            {
                const D3D12_INDEX_BUFFER_VIEW ibView = rhiGeometryDesc.indexBufferBinding->buffer->GetIndexBufferView(
                    rhiGeometryDesc.indexBufferBinding->binding);
                geometryDesc.Triangles.IndexBuffer = ibView.BufferLocation;
                geometryDesc.Triangles.IndexCount =
                    ibView.SizeInBytes / *rhiGeometryDesc.indexBufferBinding->binding.strideByteSize;
                geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            }
            else
            {
                geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
            }

            if (rhiGeometryDesc.transform.has_value())
            {
                geometryDesc.Triangles.Transform3x4 = rhiGeometryDesc.transform->buffer->GetGPUVirtualAddress() +
                                                      rhiGeometryDesc.transform->binding.offsetByteSize.value_or(0);
            }
        }
        else if (desc.type == ASGeometryType::AABBs)
        {
            const RHIBufferBinding& aabbBinding = *rhiGeometryDesc.aabbBufferBinding;
            const u32 aabbCount = *aabbBinding.binding.rangeByteSize / *aabbBinding.binding.strideByteSize;
            VEX_ASSERT(aabbCount > 0, "AABB geometry must have atleast one AABB.");
            VEX_ASSERT(aabbBinding.binding.strideByteSize.value_or(sizeof(D3D12_RAYTRACING_AABB)) ==
                           sizeof(D3D12_RAYTRACING_AABB),
                       "AABB stride must be 24 bytes (6 floats: MinX, MinY, MinZ, MaxX, MaxY, MaxZ)");
            D3D12_GPU_VIRTUAL_ADDRESS virtualAddress =
                aabbBinding.buffer->GetGPUVirtualAddress() + aabbBinding.binding.offsetByteSize.value_or(0);
            VEX_ASSERT(IsAligned<u64>(virtualAddress, D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT),
                       "Virtual address for aabb buffer must be aligned to D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT.");
            geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
            geometryDesc.AABBs = {
                .AABBCount = aabbCount,
                .AABBs = {
                    .StartAddress = virtualAddress,
                    .StrideInBytes = sizeof(D3D12_RAYTRACING_AABB),
                },
            };
        }
        else
        {
            VEX_ASSERT(false, "Unsupported BLAS geometry type.");
        }

        geometryDescs.push_back(std::move(geometryDesc));
    }
}

D3D12_RAYTRACING_GEOMETRY_FLAGS ASGeometryFlagsToDX12GeometryFlags(ASGeometry::Flags flags)
{
    return static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(flags);
}

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ASBuildFlagsToDX12ASBuildFlags(ASBuild::Flags flags)
{
    return static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(flags);
}

u32 ASInstanceFlagsToDX12InstanceFlags(ASInstance::Flags flags)
{
    u32 dxFlags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    if (flags & ASInstance::TriangleCullDisable)
    {
        dxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
    }
    if (flags & ASInstance::TriangleFrontCounterClockwise)
    {
        dxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    }
    if (flags & ASInstance::ForceOpaque)
    {
        dxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
    }
    if (flags & ASInstance::ForceNonOpaque)
    {
        dxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
    }
    return dxFlags;
}

} // namespace vex::dx12