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

public:
    DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc);
    bool CanBeMapped() override;
    std::span<u8> Map() override;
    void UnMap() override;
};

} // namespace vex::dx12
