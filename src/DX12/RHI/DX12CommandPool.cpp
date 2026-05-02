#include "DX12CommandPool.h"

#include <Vex/Utility/Formattable.h>

#include <Vex/Logger.h>
#include <VexMacros.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/Utility/WString.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12CommandPool::DX12CommandPool(RHI& rhi, const ComPtr<DX12Device>& device)
    : RHICommandPoolBase{ rhi }
    , device{ device }
{
}

NonNullPtr<RHICommandList> DX12CommandPool::GetOrCreateCommandList(QueueType queueType)
{
    DX12CommandList* cmdListPtr = nullptr;

    auto& pool = GetCommandLists(queueType);
    if (auto res = std::find_if(pool.begin(),
                                pool.end(),
                                [](const std::unique_ptr<DX12CommandList>& cmdList)
                                { return cmdList->GetState() == RHICommandListState::Available; });
        res != pool.end())
    {
        // Reserve the available command list.
        cmdListPtr = res->get();
    }
    else
    {
        // No more available command lists, create and return a new one.
        pool.push_back(std::make_unique<DX12CommandList>(device, queueType));
        cmdListPtr = pool.back().get();
#if !VEX_SHIPPING
        chk << cmdListPtr->GetNativeCommandList()->SetName(
            StringToWString(std::format("CommandList: {}_{}", magic_enum::enum_name(queueType), pool.size() - 1))
                .c_str());
#endif
        VEX_LOG(Verbose, "Created new commandlist for queue {}", magic_enum::enum_name(queueType));
    }

    VEX_ASSERT(cmdListPtr != nullptr);
    cmdListPtr->SetState(RHICommandListState::Recording);

    return NonNullPtr(cmdListPtr);
}

} // namespace vex::dx12