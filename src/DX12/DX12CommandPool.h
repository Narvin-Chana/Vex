#pragma once

#include <array>
#include <mutex>
#include <vector>

#include <RHIFwd.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHICommandPool.h>
#include <Vex/UniqueHandle.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

// The DX12 command pool uses a command list reuse strategy, since each command list possesses its own memory.
class DX12CommandPool : public RHICommandPool
{
public:
    DX12CommandPool(const ComPtr<DX12Device>& device);
    virtual ~DX12CommandPool() override = default;
    virtual RHICommandList* CreateCommandList(CommandQueueType queueType) override;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) override;
    virtual void ReclaimAllCommandListMemory() override;

private:
    std::vector<UniqueHandle<RHICommandList>>& GetAvailableCommandLists(CommandQueueType queueType);
    std::vector<UniqueHandle<RHICommandList>>& GetOccupiedCommandLists(CommandQueueType queueType);

    std::mutex poolMutex;

    ComPtr<DX12Device> device;
    std::array<std::vector<UniqueHandle<RHICommandList>>, CommandQueueTypes::Count> perQueueAvailableCommandLists;
    std::array<std::vector<UniqueHandle<RHICommandList>>, CommandQueueTypes::Count> perQueueOccupiedCommandLists;
};

} // namespace vex::dx12