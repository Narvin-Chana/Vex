#include "DX12CommandPool.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Debug.h>
#include <Vex/Logger.h>

#include <DX12/DX12CommandList.h>

namespace vex::dx12
{

DX12CommandPool::DX12CommandPool(const ComPtr<DX12Device>& device)
    : device{ device }
{
}

RHICommandList* DX12CommandPool::CreateCommandList(CommandQueueType queueType)
{
    std::scoped_lock lock(poolMutex);

    auto& available = GetAvailableCommandLists(queueType);
    auto& occupied = GetOccupiedCommandLists(queueType);

    UniqueHandle<DX12CommandList> cmdList;
    if (!available.empty())
    {
        // Reserve available command list.
        cmdList = std::move(available.back());
        available.pop_back();
    }
    else
    {
        // No more available command lists, create and return a new one.
        cmdList = MakeUnique<DX12CommandList>(device, queueType);
        VEX_LOG(Info, "Created new commandlist for queue {}", magic_enum::enum_name(queueType));
    }

    VEX_ASSERT(cmdList != nullptr);

    occupied.push_back(std::move(cmdList));
    return occupied.back().get();
}

void DX12CommandPool::ReclaimCommandListMemory(CommandQueueType queueType)
{
    std::scoped_lock lock(poolMutex);

    auto& occupied = GetOccupiedCommandLists(queueType);

    if (occupied.size())
    {
        auto& available = GetAvailableCommandLists(queueType);
#if 0
        VEX_LOG(Info,
                "Reclaimed {} cmd lists for queue {}",
                occupied.size(),
                magic_enum::enum_name(queueType));
#endif
        available.reserve(available.size() + occupied.size());
        available.insert(available.end(),
                         std::make_move_iterator(occupied.begin()),
                         std::make_move_iterator(occupied.end()));
        occupied.clear();
    }
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