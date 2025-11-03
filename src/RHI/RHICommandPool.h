#pragma once

#include <array>
#include <span>
#include <utility>
#include <vector>

#include <Vex/NonNullPtr.h>
#include <Vex/Synchronization.h>
#include <Vex/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class RHICommandPoolBase
{
public:
    RHICommandPoolBase(RHI& rhi);
    // Available -> Recording
    virtual NonNullPtr<RHICommandList> GetOrCreateCommandList(QueueType queueType) = 0;
    // Recording -> Submitted
    void OnCommandListsSubmitted(std::span<NonNullPtr<RHICommandList>> submits, std::span<SyncToken> syncTokens);
    // Submitted -> Available
    void ReclaimCommandLists();

protected:
    std::vector<UniqueHandle<RHICommandList>>& GetCommandLists(QueueType queueType);

    NonNullPtr<RHI> rhi;
    std::array<std::vector<UniqueHandle<RHICommandList>>, QueueTypes::Count> commandListsPerQueue;
};

} // namespace vex