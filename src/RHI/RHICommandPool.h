#pragma once

#include <array>
#include <span>
#include <utility>
#include <vector>

#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>
#include <Vex/Synchronization.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class RHICommandPoolBase
{
public:
    RHICommandPoolBase(RHI& rhi);
    // Available -> Recording
    virtual NonNullPtr<RHICommandList> GetOrCreateCommandList(CommandQueueType queueType) = 0;
    // Recording -> Submitted
    void OnCommandListsSubmitted(std::span<NonNullPtr<RHICommandList>> submits, std::span<SyncToken> syncTokens);
    // Submitted -> Available
    void ReclaimCommandLists();

protected:
    std::vector<UniqueHandle<RHICommandList>>& GetCommandLists(CommandQueueType queueType);

    RHI& rhi;
    std::array<std::vector<UniqueHandle<RHICommandList>>, CommandQueueTypes::Count> commandListsPerQueue;
};

} // namespace vex