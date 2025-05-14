#include "DX12CommandList.h"

#include <Vex/Bindings.h>
#include <Vex/Logger.h>

#include <DX12/DX12PipelineState.h>
#include <DX12/DX12ResourceLayout.h>
#include <DX12/DX12Texture.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12CommandList::DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type)
    : type{ type }
{
    D3D12_COMMAND_LIST_TYPE d3dType;
    switch (type)
    {
    case CommandQueueType::Copy:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY;
        break;

    case CommandQueueType::Compute:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;

    case CommandQueueType::Graphics:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;

    default:
        VEX_LOG(Fatal, "Invalid command queue type passed to command list creation.");
    }

    // Create CommandList1 creates the command list closed by default.
    chk << device->CreateCommandList1(0, d3dType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));
    chk << device->CreateCommandAllocator(d3dType, IID_PPV_ARGS(&commandAllocator));
}

bool DX12CommandList::IsOpen() const
{
    return isOpen;
}

void DX12CommandList::Open()
{
    if (isOpen)
    {
        VEX_LOG(Fatal, "Attempting to open an already open command list.");
        return;
    }

    chk << commandAllocator->Reset();
    chk << commandList->Reset(commandAllocator.Get(), nullptr);
    isOpen = true;
}

void DX12CommandList::Close()
{
    if (!isOpen)
    {
        VEX_LOG(Fatal, "Attempting to close an already closed command list.");
        return;
    }

    chk << commandList->Close();
    isOpen = false;
}

void DX12CommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    D3D12_VIEWPORT viewport[] = { {
        .TopLeftX = x,
        .TopLeftY = y,
        .Width = width,
        .Height = height,
        .MinDepth = minDepth,
        .MaxDepth = maxDepth,
    } };
    commandList->RSSetViewports(1, viewport);
}

void DX12CommandList::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    D3D12_RECT rect[] = { {
        .left = x,
        .top = y,
        .right = static_cast<LONG>(width),
        .bottom = static_cast<LONG>(height),
    } };
    commandList->RSSetScissorRects(1, rect);
}

void DX12CommandList::SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState)
{
    const auto& pso = reinterpret_cast<const DX12GraphicsPipelineState&>(graphicsPipelineState);
    commandList->SetPipelineState(pso.graphicsPSO.Get());
}

void DX12CommandList::SetPipelineState(const RHIComputePipelineState& computePipelineState)
{
    const auto& pso = reinterpret_cast<const DX12ComputePipelineState&>(computePipelineState);
    commandList->SetPipelineState(pso.computePSO.Get());
}

void DX12CommandList::SetLayout(RHIResourceLayout& layout)
{
    auto globalRootSignature = reinterpret_cast<DX12ResourceLayout&>(layout).GetRootSignature().Get();

    // TEMP
    commandList->SetDescriptorHeaps(1, reinterpret_cast<DX12ResourceLayout&>(layout).descriptorHeap.GetAddressOf());

    switch (type)
    {
    case CommandQueueType::Graphics:
        commandList->SetGraphicsRootSignature(globalRootSignature);
        // TEMP
        commandList->SetGraphicsRootDescriptorTable(
            0,
            reinterpret_cast<DX12ResourceLayout&>(layout).descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    case CommandQueueType::Compute:
        commandList->SetComputeRootSignature(globalRootSignature);
        // TEMP
        commandList->SetComputeRootDescriptorTable(
            0,
            reinterpret_cast<DX12ResourceLayout&>(layout).descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    case CommandQueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::SetLayoutLocalConstants(const RHIResourceLayout& layout,
                                              std::span<const ConstantBinding> constants)
{
    u32 localConstantsByteSize = 0;
    // Compute total size of constants (and make sure the constants fit in local constants).
    for (const auto& binding : constants)
    {
        localConstantsByteSize += binding.size;
    }

    const auto& globalRootSignature = reinterpret_cast<const DX12ResourceLayout&>(layout);
    if (localConstantsByteSize > globalRootSignature.GetMaxLocalConstantSize())
    {
        VEX_LOG(Fatal,
                "Unable to bind local constants, you have surpassed the limit DX12 allows for in root signatures.");
        return;
    }

    std::vector<u8> dataToUpload(localConstantsByteSize);
    u8* currPtr = dataToUpload.data();
    for (const auto& binding : constants)
    {
        std::memcpy(currPtr, binding.data, binding.size);
        currPtr += binding.size;
    }

    auto DivideAndRoundUp = [](u32 x, u32 y) { return (x + y - 1) / y; };

    // Data is stored in the form of 32bit chunks, so there is still a change our local data is too fat to fit in root
    // constant storage.
    u32 finalSize = DivideAndRoundUp(localConstantsByteSize, sizeof(DWORD));
    if (finalSize > globalRootSignature.GetMaxLocalConstantSize())
    {
        VEX_LOG(Fatal,
                "Unable to bind local constants, you have surpassed the limit DX12 allows for in root signatures.");
    }

    if (finalSize == 0)
    {
        return;
    }

    std::vector<u8> padding =
        std::vector<u8>((globalRootSignature.GetMaxLocalConstantSize() - finalSize) * sizeof(DWORD));

    switch (type)
    {
        // TODO: start at 2 is because of the temp UAV for hello triangle.
    case CommandQueueType::Graphics:
        commandList->SetGraphicsRoot32BitConstants(2, finalSize, dataToUpload.data(), 0);
        commandList->SetGraphicsRoot32BitConstants(finalSize + 1,
                                                   DivideAndRoundUp(padding.size(), sizeof(DWORD)),
                                                   padding.data(),
                                                   0);
    case CommandQueueType::Compute:
        commandList->SetComputeRoot32BitConstants(2, finalSize, dataToUpload.data(), 0);
        commandList->SetComputeRoot32BitConstants(finalSize + 1,
                                                  DivideAndRoundUp(padding.size(), sizeof(DWORD)),
                                                  padding.data(),
                                                  0);
    case CommandQueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::Dispatch(const std::array<u32, 3>& groupCount, RHIResourceLayout& layout, RHITexture& backbuffer)
{
    switch (type)
    {
    case CommandQueueType::Graphics:
    case CommandQueueType::Compute:
        commandList->Dispatch(groupCount[0], groupCount[1], groupCount[2]);
    case CommandQueueType::Copy:
    default:
        break;
    }

    // TEMP CODE to copy the UAV to the backbuffer.
    ID3D12Resource* bb = reinterpret_cast<DX12Texture&>(backbuffer).GetRawTexture();
    ID3D12Resource* uavTexture = reinterpret_cast<const DX12ResourceLayout&>(layout).uavTexture.Get();

    {
        D3D12_RESOURCE_BARRIER uavToSource = CD3DX12_RESOURCE_BARRIER::Transition(uavTexture,
                                                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                                                  D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_RESOURCE_BARRIER backbufferToDest =
            CD3DX12_RESOURCE_BARRIER::Transition(bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_RESOURCE_BARRIER firstBarriers[] = { uavToSource, backbufferToDest };
        commandList->ResourceBarrier(2, firstBarriers);
    }

    commandList->CopyResource(bb, uavTexture);

    {
        D3D12_RESOURCE_BARRIER sourceToUAV =
            CD3DX12_RESOURCE_BARRIER::Transition(uavTexture,
                                                 D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        D3D12_RESOURCE_BARRIER backbufferToPresent =
            CD3DX12_RESOURCE_BARRIER::Transition(bb, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
        D3D12_RESOURCE_BARRIER finalBarriers[] = { sourceToUAV, backbufferToPresent };
        commandList->ResourceBarrier(2, finalBarriers);
    }
}

CommandQueueType DX12CommandList::GetType() const
{
    return type;
}

} // namespace vex::dx12