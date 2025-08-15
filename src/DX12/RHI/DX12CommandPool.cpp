#include "DX12CommandPool.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Debug.h>
#include <Vex/Logger.h>

namespace vex::dx12
{

DX12CommandPool::DX12CommandPool(const ComPtr<DX12Device>& device)
    : device{ device }
{
}

RHICommandList* DX12CommandPool::CreateCommandList(CommandQueueType queueType)
{
    UniqueHandle<DX12CommandList> cmdList = nullptr;

    auto& pool = GetAvailableCommandLists(queueType);
    if (!pool.empty())
    {
        // Reserve available command list.
        cmdList = std::move(pool.back());
        pool.pop_back();
    }
    else
    {
        // No more available command lists, create and return a new one.
        cmdList = MakeUnique<DX12CommandList>(device, queueType);
        VEX_LOG(Info, "Created new commandlist for queue {}", magic_enum::enum_name(queueType));
    }

    VEX_ASSERT(cmdList != nullptr);

    auto& occupied = GetOccupiedCommandLists(queueType);
    occupied.push_back(std::move(cmdList));
    return occupied.back().get();
}

void DX12CommandPool::ReclaimCommandListMemory(CommandQueueType queueType)
{
    auto& occupied = GetOccupiedCommandLists(queueType);
    if (occupied.empty())
    {
        return;
    }

    auto& pool = GetAvailableCommandLists(queueType);
    std::size_t availableCmdListSize = pool.size();
    VEX_LOG(Verbose, "Reclaimed {} cmd lists for queue {}", occupied.size(), magic_enum::enum_name(queueType));

    pool.resize(availableCmdListSize + occupied.size());
    std::move(occupied.begin(), occupied.end(), pool.data() + availableCmdListSize);
    occupied.clear();
}

void DX12CommandPool::ReclaimAllCommandListMemory()
{
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        ReclaimCommandListMemory(queueType);
    }
}

std::vector<UniqueHandle<DX12CommandList>>& DX12CommandPool::GetAvailableCommandLists(CommandQueueType queueType)
{
    return perQueueAvailableCommandLists[queueType];
}

std::vector<UniqueHandle<DX12CommandList>>& DX12CommandPool::GetOccupiedCommandLists(CommandQueueType queueType)
{
    return perQueueOccupiedCommandLists[queueType];
}

} // namespace vex::dx12