#pragma once
#include <RHI/RHITimestampQueryPool.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12TimestampQueryPool final : public RHITimestampQueryPoolBase
{
    ComPtr<ID3D12QueryHeap> heap;
    NonNullPtr<RHI> rhi;

protected:
    double GetTimestampPeriod(QueueType queueType) override;

public:
    DX12TimestampQueryPool(RHI& rhi, RHIAllocator& allocator);

    ID3D12QueryHeap* GetNativeQueryHeap()
    {
        return heap.Get();
    }
};

} // namespace vex::dx12
