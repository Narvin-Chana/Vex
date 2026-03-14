#pragma once

#include <array>
#include <Vex/Containers/Span.h>
#include <utility>
#include <vector>

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Synchronization.h>


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
    void OnCommandListsSubmitted(Span<const NonNullPtr<RHICommandList>> submits, Span<const SyncToken> syncTokens);
    // Submitted -> Available
    void ReclaimCommandLists();

protected:
    std::vector<std::unique_ptr<RHICommandList>>& GetCommandLists(QueueType queueType);

    NonNullPtr<RHI> rhi;
    std::array<std::vector<std::unique_ptr<RHICommandList>>, QueueTypes::Count> commandListsPerQueue;
};

} // namespace vex