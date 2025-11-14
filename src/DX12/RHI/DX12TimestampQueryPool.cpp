#include "DX12TimestampQueryPool.h"

#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12CommandList.h>
#include <DX12/RHI/DX12RHI.h>

namespace vex::dx12
{

double DX12TimestampQueryPool::GetTimestampPeriod(QueueType queueType)
{
    u64 frequency;
    rhi->GetNativeQueue(queueType)->GetTimestampFrequency(&frequency);
    return static_cast<double>(frequency);
}

DX12TimestampQueryPool::DX12TimestampQueryPool(RHI& rhi, RHIAllocator& allocator)
    : RHITimestampQueryPoolBase(rhi, allocator)
    , rhi{ rhi }
{
    D3D12_QUERY_HEAP_DESC heapDesc{};
    heapDesc.Count = MaxInFlightQueriesCount * 2;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    chk << rhi.GetNativeDevice()->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(heap.GetAddressOf()));
}

} // namespace vex::dx12