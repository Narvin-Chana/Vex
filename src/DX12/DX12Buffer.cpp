#include "DX12Buffer.h"

#include "Vex/Buffer.h"
#include "Vex/Debug.h"

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc)
    : RHIBuffer(desc)
{
}

UniqueHandle<RHIMappedBufferMemory> DX12Buffer::GetMappedMemory()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

RHIBuffer* DX12Buffer::GetStagingBuffer()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

void DX12Buffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex::dx12
