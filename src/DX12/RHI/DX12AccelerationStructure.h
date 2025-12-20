#pragma once

#include <optional>

#include <RHI/RHIAccelerationStructure.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12AccelerationStructure final : public RHIAccelerationStructureBase
{
public:
    DX12AccelerationStructure(ComPtr<DX12Device>& device, const BLASDesc& desc);
    DX12AccelerationStructure(ComPtr<DX12Device>& device, const TLASDesc& desc);

    const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& GetGeometryDescs() const
    {
        return geometryDescs;
    }

    virtual const RHIAccelerationStructureBuildInfo& SetupBLASBuild(RHIAllocator& allocator,
                                                                    const RHIBLASBuildDesc& desc) override;
    virtual const RHIAccelerationStructureBuildInfo& SetupTLASBuild(RHIAllocator& allocator,
                                                                    const RHITLASBuildDesc& desc) override;

private:
    void InitRayTracingGeometryDesc(const RHIBLASBuildDesc& desc);

    ComPtr<DX12Device> device;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
};

} // namespace vex::dx12