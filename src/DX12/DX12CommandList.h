#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHICommandList.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12CommandList : public RHICommandList
{
public:
    DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type);
    virtual ~DX12CommandList() = default;

    virtual bool IsOpen() const override;

    virtual void Open() override;
    virtual void Close() override;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) override;

    virtual CommandQueueType GetType() const override;

private:
    CommandQueueType type;
    ComPtr<ID3D12GraphicsCommandList10> commandList;

    // Underlying memory of the command list.
    ComPtr<ID3D12CommandAllocator> commandAllocator;

    bool isOpen = false;

    friend class DX12RHI;
};

} // namespace vex::dx12