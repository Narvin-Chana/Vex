#include "DX12AccelerationStructure.h"

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
        .usage = BufferUsage::AccelerationStructure,
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
        .usage = BufferUsage::AccelerationStructure,
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
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = ASGeometryFlagsToDX12GeometryFlags(rhiGeometryDesc.flags);
        const D3D12_VERTEX_BUFFER_VIEW vbView = rhiGeometryDesc.vertexBufferBinding.buffer->GetVertexBufferView(
            rhiGeometryDesc.vertexBufferBinding.binding);
        geometryDesc.Triangles.VertexBuffer = {
            .StartAddress = vbView.BufferLocation,
            .StrideInBytes = vbView.StrideInBytes,
        };
        geometryDesc.Triangles.VertexCount = vbView.SizeInBytes / vbView.StrideInBytes;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        // TODO(https://trello.com/c/srGndUSP): Handle other vertex formats, this should be cross-referenced with Vulkan
        // to make sure only formats supported by both APIs are accepted.
        VEX_ASSERT(vbView.StrideInBytes == sizeof(float) * 3,
                   "Vex currently does not support Vertices with a larger stride than 12 bytes.");

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