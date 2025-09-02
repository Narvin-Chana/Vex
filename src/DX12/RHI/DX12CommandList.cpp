#include "DX12CommandList.h"

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/RHIBindings.h>
#include <Vex/Texture.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12GraphicsPipeline.h>
#include <DX12/DX12States.h>
#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12Buffer.h>
#include <DX12/RHI/DX12PipelineState.h>
#include <DX12/RHI/DX12ResourceLayout.h>
#include <DX12/RHI/DX12Texture.h>

namespace vex::dx12
{

DX12CommandList::DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type)
    : device{ device }
    , type{ type }
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
    commandList->SetPipelineState(graphicsPipelineState.graphicsPSO.Get());
}

void DX12CommandList::SetPipelineState(const RHIComputePipelineState& computePipelineState)
{
    commandList->SetPipelineState(computePipelineState.computePSO.Get());
}

void DX12CommandList::SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState)
{
    commandList->SetPipelineState1(rayTracingPipelineState.stateObject.Get());
}

void DX12CommandList::SetLayout(RHIResourceLayout& layout)
{
    ID3D12RootSignature* globalRootSignature = layout.GetRootSignature().Get();

    // Set root signature.
    switch (type)
    {
    case CommandQueueType::Graphics:
        commandList->SetGraphicsRootSignature(globalRootSignature);
    case CommandQueueType::Compute:
        commandList->SetComputeRootSignature(globalRootSignature);
    case CommandQueueType::Copy:
    default:
        break;
    }

    std::span<const u8> localConstantsData = layout.GetLocalConstantsData();
    if (localConstantsData.empty())
    {
        return;
    }

    switch (type)
    {
    case CommandQueueType::Graphics:
        // Set local constants (in first slot of root signature).
        commandList->SetGraphicsRoot32BitConstants(0,
                                                   static_cast<u32>(localConstantsData.size()),
                                                   localConstantsData.data(),
                                                   0);
    case CommandQueueType::Compute:
        // Set local constants (in first slot of root signature).
        commandList->SetComputeRoot32BitConstants(0,
                                                  static_cast<u32>(localConstantsData.size()),
                                                  localConstantsData.data(),
                                                  0);
    case CommandQueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& layout)
{
    commandList->SetDescriptorHeaps(1, descriptorPool.gpuHeap.GetNativeDescriptorHeap().GetAddressOf());
}

void DX12CommandList::SetInputAssembly(InputAssembly inputAssembly)
{
    commandList->IASetPrimitiveTopology(GraphicsPipeline::GetDX12PrimitiveTopologyFromInputAssembly(inputAssembly));
}

void DX12CommandList::ClearTexture(const RHITextureBinding& binding,
                                   TextureUsage::Type usage,
                                   const TextureClearValue& clearValue)
{
    const TextureBinding& clearBinding = binding.binding;

    // Clearing in DX12 allows for multiple slices to be cleared, however you cannot clear multiple mips with one
    // call.
    // Instead we iterate on the mips passed in by the user.

    if (usage == TextureUsage::RenderTarget)
    {
        DX12TextureView dxTextureView{ clearBinding };
        dxTextureView.usage = TextureUsage::RenderTarget;
        dxTextureView.mipCount = 1;

        for (u32 mip = clearBinding.mipBias;
             mip < ((clearBinding.mipCount == 0) ? 1 : (clearBinding.mipBias + clearBinding.mipCount));
             ++mip)
        {
            dxTextureView.mipBias = mip;
            VEX_ASSERT(clearValue.flags & TextureClear::ClearColor,
                       "Clearing the color requires the TextureClear::ClearColor flag for texture: {}.",
                       desc.name);
            commandList->ClearRenderTargetView(binding.texture->GetOrCreateRTVDSVView(dxTextureView),
                                               clearValue.color.data(),
                                               0,
                                               nullptr);
        }
    }
    else if (usage == TextureUsage::DepthStencil)
    {
        DX12TextureView dxTextureView{ clearBinding };
        dxTextureView.usage = TextureUsage::DepthStencil;
        dxTextureView.mipCount = 1;
        for (u32 mip = clearBinding.mipBias;
             mip < ((clearBinding.mipCount == 0) ? 1 : (clearBinding.mipBias + clearBinding.mipCount));
             ++mip)
        {
            dxTextureView.mipBias = mip;
            D3D12_CLEAR_FLAGS clearFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
            if (clearValue.flags & TextureClear::ClearDepth)
            {
                clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
            }
            if (clearValue.flags & TextureClear::ClearStencil)
            {
                clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
            }
            VEX_ASSERT(std::to_underlying(clearFlags) != 0,
                       "Clear flags for the depth-stencil cannot be 0, you must either clear depth, stencil, or both "
                       "for texture: {}!",
                       clearBinding.texture.description.name);

            commandList->ClearDepthStencilView(binding.texture->GetOrCreateRTVDSVView(dxTextureView),
                                               clearFlags,
                                               clearValue.depth,
                                               clearValue.stencil,
                                               0,
                                               nullptr);
        }
    }
    else
    {
        VEX_LOG(Fatal,
                "The usage of the passed binding \"{}\" doesn't support clearing. Make sure you specify "
                "the correct usage.",
                clearBinding.texture.description.name);
    }
}

void DX12CommandList::Transition(RHITexture& texture, RHITextureState::Flags newState)
{
    D3D12_RESOURCE_STATES currentDX12State = RHITextureStateToDX12State(texture.GetCurrentState());
    D3D12_RESOURCE_STATES newDX12State = RHITextureStateToDX12State(newState);

    texture.SetCurrentState(newState);
    // Nothing to do if the states are already equal (we compare raw API states, due to them not mapping 1:1 to Vex
    // ones).
    if (currentDX12State == newDX12State)
    {
        return;
    }

    CD3DX12_RESOURCE_BARRIER resourceBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(texture.GetRawTexture(), currentDX12State, newDX12State);

    commandList->ResourceBarrier(1, &resourceBarrier);
}

void DX12CommandList::Transition(RHIBuffer& buffer, RHIBufferState::Flags newState)
{
    // Nothing to do if the states are already equal.
    if (buffer.GetCurrentState() == newState)
    {
        return;
    }

    D3D12_RESOURCE_STATES currentDX12State = RHIBufferStateToDX12State(buffer.GetCurrentState());
    D3D12_RESOURCE_STATES newDX12State = RHIBufferStateToDX12State(newState);
    D3D12_RESOURCE_BARRIER resourceBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(buffer.GetRawBuffer(), currentDX12State, newDX12State);

    buffer.SetCurrentState(newState);

    commandList->ResourceBarrier(1, &resourceBarrier);
}

void DX12CommandList::Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs)
{
    std::vector<D3D12_RESOURCE_BARRIER> transitionBarriers;
    transitionBarriers.reserve(textureNewStatePairs.size());
    for (auto& [texture, newState] : textureNewStatePairs)
    {
        D3D12_RESOURCE_STATES currentDX12State = RHITextureStateToDX12State(texture.GetCurrentState());
        D3D12_RESOURCE_STATES newDX12State = RHITextureStateToDX12State(newState);
        texture.SetCurrentState(newState);
        // Nothing to do if the states are already equal (we compare raw API states, due to them not mapping 1:1 to Vex
        // ones).
        if (newDX12State == currentDX12State)
        {
            continue;
        }
        D3D12_RESOURCE_BARRIER resourceBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(texture.GetRawTexture(), currentDX12State, newDX12State);
        transitionBarriers.push_back(std::move(resourceBarrier));
    }

    if (!transitionBarriers.empty())
    {
        commandList->ResourceBarrier(transitionBarriers.size(), transitionBarriers.data());
    }
}

void DX12CommandList::Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs)
{
    std::vector<D3D12_RESOURCE_BARRIER> transitionBarriers;
    transitionBarriers.reserve(bufferNewStatePairs.size());
    for (auto& [buffer, newState] : bufferNewStatePairs)
    {
        D3D12_RESOURCE_STATES currentDX12State = RHIBufferStateToDX12State(buffer.GetCurrentState());
        D3D12_RESOURCE_STATES newDX12State = RHIBufferStateToDX12State(newState);
        if (newDX12State == currentDX12State)
        {
            continue;
        }
        D3D12_RESOURCE_BARRIER resourceBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(buffer.GetRawBuffer(), currentDX12State, newDX12State);

        buffer.SetCurrentState(newState);

        transitionBarriers.push_back(std::move(resourceBarrier));
    }

    if (!transitionBarriers.empty())
    {
        commandList->ResourceBarrier(transitionBarriers.size(), transitionBarriers.data());
    }
}
void DX12CommandList::BeginRendering(const RHIDrawResources& resources)
{
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    rtvHandles.reserve(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

    std::optional<CD3DX12_CPU_DESCRIPTOR_HANDLE> dsvHandle;

    for (const auto& [binding, texture] : resources.renderTargets)
    {
        DX12TextureView rtvView{ binding };
        rtvView.usage = TextureUsage::RenderTarget;
        rtvHandles.emplace_back(texture->GetOrCreateRTVDSVView(rtvView));
    }

    if (resources.depthStencil)
    {
        DX12TextureView dsvView{ resources.depthStencil->binding };
        dsvView.usage = TextureUsage::DepthStencil;
        dsvHandle = resources.depthStencil->texture->GetOrCreateRTVDSVView(dsvView);
    }

    // Bind RTV and DSVs
    if (type == CommandQueueType::Graphics)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE* dsvHandlePtr = dsvHandle.has_value() ? &dsvHandle.value() : nullptr;
        commandList->OMSetRenderTargets(static_cast<u32>(rtvHandles.size()), rtvHandles.data(), true, dsvHandlePtr);
    }
    else
    {
        VEX_ASSERT(!dsvHandle.has_value() && rtvHandles.empty(),
                   "Cannot bind a depth stencil or render target to a non-graphics queue CommandList.");
    }
}

void DX12CommandList::EndRendering()
{
    // Nothing to do here
}

void DX12CommandList::Draw(u32 vertexCount, u32 instanceCount, u32 vertexOffset, u32 instanceOffset)
{
    if (type != CommandQueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    commandList->DrawInstanced(vertexCount, instanceCount, vertexOffset, instanceOffset);
}

void DX12CommandList::DrawIndexed(
    u32 indexCount, u32 instanceCount, u32 indexOffset, u32 vertexOffset, u32 instanceOffset)
{
    if (type != CommandQueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    commandList->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
}

void DX12CommandList::SetVertexBuffers(u32 startSlot, std::span<RHIBufferBinding> vertexBuffers)
{
    if (type != CommandQueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
    views.reserve(vertexBuffers.size());
    for (auto& [binding, buffer] : vertexBuffers)
    {
        views.push_back(buffer->GetVertexBufferView(binding));
    }
    commandList->IASetVertexBuffers(startSlot, views.size(), views.data());
}

void DX12CommandList::SetIndexBuffer(const RHIBufferBinding& indexBuffer)
{
    if (type != CommandQueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer.buffer->GetIndexBufferView(indexBuffer.binding);
    commandList->IASetIndexBuffer(&indexBufferView);
}

void DX12CommandList::Dispatch(const std::array<u32, 3>& groupCount)
{
    switch (type)
    {
    case CommandQueueType::Graphics:
    case CommandQueueType::Compute:
        commandList->Dispatch(groupCount[0], groupCount[1], groupCount[2]);
        break;
    case CommandQueueType::Copy:
    default:
        VEX_LOG(Fatal, "Cannot use dispatch with a non-compute capable command queue.");
        break;
    }
}

void DX12CommandList::TraceRays(const std::array<u32, 3>& widthHeightDepth,
                                const RHIRayTracingPipelineState& rayTracingPipelineState)
{
    // Attach shader record and tables.
    D3D12_DISPATCH_RAYS_DESC rayDesc{
        .Width = widthHeightDepth[0],
        .Height = widthHeightDepth[1],
        .Depth = widthHeightDepth[2],
    };

    rayTracingPipelineState.PrepareDispatchRays(rayDesc);

    switch (type)
    {
    case CommandQueueType::Graphics:
    case CommandQueueType::Compute:
        commandList->DispatchRays(&rayDesc);
    case CommandQueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::Copy(RHITexture& src, RHITexture& dst)
{
    VEX_ASSERT(src.GetDescription().width == dst.GetDescription().width &&
                   src.GetDescription().height == dst.GetDescription().height &&
                   src.GetDescription().depthOrArraySize == dst.GetDescription().depthOrArraySize &&
                   src.GetDescription().mips == dst.GetDescription().mips &&
                   src.GetDescription().format == dst.GetDescription().format,
               "The two textures must be compatible in order to Copy to be useable.");
    commandList->CopyResource(dst.GetRawTexture(), src.GetRawTexture());
}

void DX12CommandList::Copy(RHITexture& src, RHITexture& dst, std::span<TextureCopyDescription> regionMappings)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void DX12CommandList::Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDescription& regionMappings)
{
    commandList->CopyBufferRegion(dst.GetRawBuffer(),
                                  regionMappings.dstOffset,
                                  src.GetRawBuffer(),
                                  regionMappings.srcOffset,
                                  regionMappings.size);
}
void DX12CommandList::Copy(RHIBuffer& src, RHITexture& dst, std::span<BufferToTextureCopyDescription> regionMappings)
{
    VEX_NOT_YET_IMPLEMENTED();
}

CommandQueueType DX12CommandList::GetType() const
{
    return type;
}

} // namespace vex::dx12