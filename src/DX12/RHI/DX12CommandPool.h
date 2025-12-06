#pragma once

#include <array>
#include <mutex>
#include <vector>

#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHI.h>
#include <RHI/RHICommandPool.h>

#include <DX12/RHI/DX12CommandList.h>

namespace vex::dx12
{

// The DX12 command pool uses a command list reuse strategy, since each command list possesses its own memory.
class DX12CommandPool final : public RHICommandPoolBase
{
public:
    DX12CommandPool(RHI& rhi, const ComPtr<DX12Device>& device);

    virtual NonNullPtr<RHICommandList> GetOrCreateCommandList(QueueType queueType) override;

    DX12CommandPool(DX12CommandPool&&) = default;
    DX12CommandPool& operator=(DX12CommandPool&&) = default;

private:
    ComPtr<DX12Device> device;
};

} // namespace vex::dx12