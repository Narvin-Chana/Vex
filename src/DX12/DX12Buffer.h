#pragma once

#include "DX12Headers.h"

#include <Vex/RHI/RHIBuffer.h>

namespace vex
{
struct BufferDescription;
}
namespace vex::dx12
{

class DX12Buffer : public RHIBuffer
{
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() override;

public:
    DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc);

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
};

} // namespace vex::dx12
