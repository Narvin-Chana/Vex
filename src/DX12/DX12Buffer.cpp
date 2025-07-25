#include "DX12Buffer.h"

#include "Vex/Buffer.h"
#include "Vex/Debug.h"

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc)
    : RHIBuffer(desc)
{
}

UniqueHandle<RHIBuffer> DX12Buffer::CreateStagingBuffer()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

std::span<u8> DX12Buffer::Map()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

void DX12Buffer::Unmap()
{
    VEX_NOT_YET_IMPLEMENTED();
}

void DX12Buffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex::dx12
