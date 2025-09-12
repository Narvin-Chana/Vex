#pragma once

#include <RHI/RHIDescriptorPool.h>

namespace vex::dx12
{

class DX12DescriptorPool final : public RHIDescriptorPoolBase
{
public:
    DX12DescriptorPool(ComPtr<DX12Device> device);

    virtual RHIBindlessDescriptorSet CreateBindlessSet() override;

private:
    ComPtr<DX12Device> device;
};
} // namespace vex::dx12