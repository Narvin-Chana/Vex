#include "DX12AccelerationStructure.h"

namespace vex::dx12
{

DX12AccelerationStructure::DX12AccelerationStructure(ComPtr<DX12Device>& device, const BLASDesc& desc)
    : RHIAccelerationStructureBase(desc)
    , device(device)
{
}

DX12AccelerationStructure::DX12AccelerationStructure(ComPtr<DX12Device>& device, const TLASDesc& desc)
    : RHIAccelerationStructureBase(desc)
    , device(device)
{
}

} // namespace vex::dx12