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

    UniqueHandle<DX12CommandList> cmdList = nullptr;

    if (!GetAvailableCommandLists(queueType).empty())
    {
        // Reserve available command list.
        cmdList = std::move(GetAvailableCommandLists(queueType).back());
        GetAvailableCommandLists(queueType).pop_back();
        VEX_LOG(Info, "GetCmdList for queue {}", magic_enum::enum_name(queueType));
    }
    else
    {
        // No more available command lists, create and return a new one.
        cmdList = MakeUnique<DX12CommandList>(device, queueType);
        VEX_LOG(Info, "Created new commandlist for queue {}", magic_enum::enum_name(queueType));
    }

    VEX_ASSERT(cmdList != nullptr);

    GetOccupiedCommandLists(queueType).push_back(std::move(cmdList));
    return GetOccupiedCommandLists(queueType).back().get();
}

void DX12CommandPool::ReclaimCommandListMemory(CommandQueueType queueType)
{
    if (GetOccupiedCommandLists(queueType).size())
    {
        std::size_t availableCmdListSize = GetAvailableCommandLists(queueType).size();
        VEX_LOG(Info,
                "Reclaimed {} cmd lists for queue {}",
                GetOccupiedCommandLists(queueType).size(),
                magic_enum::enum_name(queueType));
        GetAvailableCommandLists(queueType).resize(availableCmdListSize + GetOccupiedCommandLists(queueType).size());
        std::move(GetOccupiedCommandLists(queueType).begin(),
                  GetOccupiedCommandLists(queueType).end(),
                  GetAvailableCommandLists(queueType).data() + availableCmdListSize);
        GetOccupiedCommandLists(queueType).clear();
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