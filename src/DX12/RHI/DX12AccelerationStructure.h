#pragma once

#include <RHI/RHIAccelerationStructure.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12AccelerationStructure final : public RHIAccelerationStructureBase
{
public:
    DX12AccelerationStructure(ComPtr<DX12Device>& device, const ASDesc& desc);

private:
    ComPtr<DX12Device> device;
};

} // namespace vex::dx12