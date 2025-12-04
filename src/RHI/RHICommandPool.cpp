#include "RHICommandPool.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>

namespace vex
{

RHICommandPoolBase::RHICommandPoolBase(RHI& rhi)
    : rhi(rhi)
{
}

void RHICommandPoolBase::OnCommandListsSubmitted(Span<const NonNullPtr<RHICommandList>> submits,
                                                 Span<const SyncToken> syncTokens)
{
    // Save the state of the newly submitted command lists
    for (u32 i = 0; i < submits.size(); ++i)
    {
        auto& cmdList = submits[i];
        cmdList->SetState(RHICommandListState::Submitted);
        cmdList->SetSyncTokens(syncTokens);
    }
}

void RHICommandPoolBase::ReclaimCommandLists()
{
    for (u32 i = 0; i < QueueTypes::Count; ++i)
    {
        for (auto& pool = GetCommandLists(static_cast<QueueType>(i)); auto& cmdList : pool)
        {
            // We can only reclaim submitted command lists.
            if (cmdList->GetState() == RHICommandListState::Submitted)
            {
                bool areAllTokensComplete = true;
                for (auto tokens = cmdList->GetSyncTokens(); auto& token : tokens)
                {
                    if (!rhi->IsTokenComplete(token))
                    {
                        areAllTokensComplete = false;
                        break;
                    }
                }

                if (areAllTokensComplete)
                {
                    // Work is done, can now mark as available (and thus reclaim the command list for future CPU use).
                    cmdList->SetState(RHICommandListState::Available);
                }
            }
        }
    }
}

std::vector<UniqueHandle<RHICommandList>>& RHICommandPoolBase::GetCommandLists(QueueType queueType)
{
    return commandListsPerQueue[queueType];
}

} // namespace vex