#include "DX12Buffer.h"

#include "Vex/Buffer.h"
#include "Vex/Debug.h"

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc)
    : RHIBuffer(desc.usage, desc.size)
{
}

bool DX12Buffer::CanBeMapped()
{
    VEX_NOT_YET_IMPLEMENTED();
    return false;
}
std::span<u8> DX12Buffer::Map()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}
void DX12Buffer::UnMap()
{
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex::dx12
