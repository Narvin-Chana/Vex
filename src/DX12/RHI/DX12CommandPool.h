#pragma once

#include <array>
#include <mutex>
#include <vector>

#include <Vex/UniqueHandle.h>

#include <RHI/RHI.h>
#include <RHI/RHICommandPool.h>

#include <DX12/RHI/DX12CommandList.h>

namespace vex::dx12
{

// The DX12 command pool uses a command list reuse strategy, since each command list possesses its own memory.
class DX12CommandPool final : public RHICommandPoolBase
{
public:
    DX12CommandPool(const ComPtr<DX12Device>& device);
    virtual NonNullPtr<RHICommandList> CreateCommandList(CommandQueueType queueType) override;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) override;
    virtual void ReclaimAllCommandListMemory() override;

private:
    std::vector<UniqueHandle<DX12CommandList>>& GetAvailableCommandLists(CommandQueueType queueType);
    std::vector<UniqueHandle<DX12CommandList>>& GetOccupiedCommandLists(CommandQueueType queueType);

    ComPtr<DX12Device> device;
    std::array<std::vector<UniqueHandle<DX12CommandList>>, CommandQueueTypes::Count> perQueueAvailableCommandLists;
    std::array<std::vector<UniqueHandle<DX12CommandList>>, CommandQueueTypes::Count> perQueueOccupiedCommandLists;
};

} // namespace vex::dx12