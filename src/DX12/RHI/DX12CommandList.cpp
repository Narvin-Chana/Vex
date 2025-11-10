#include "DX12CommandList.h"

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/ByteUtils.h>
#include <Vex/Logger.h>
#include <Vex/Texture.h>
#include <Vex/Validation.h>

#include <RHI/RHIBindings.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12GraphicsPipeline.h>
#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12Barrier.h>
#include <DX12/RHI/DX12Buffer.h>
#include <DX12/RHI/DX12PipelineState.h>
#include <DX12/RHI/DX12ResourceLayout.h>
#include <DX12/RHI/DX12ScopedGPUEvent.h>
#include <DX12/RHI/DX12Texture.h>

namespace vex::dx12
{

namespace CommandList_Internal
{

static bool CanMergeBarriers(const D3D12_TEXTURE_BARRIER& a, const D3D12_TEXTURE_BARRIER& b)
{
    // Must be for the same resource
    if (a.pResource != b.pResource)
        return false;

    // Must have the same sync/access/layout transitions
    if (a.SyncBefore != b.SyncBefore || a.SyncAfter != b.SyncAfter)
        return false;

    if (a.AccessBefore != b.AccessBefore || a.AccessAfter != b.AccessAfter)
        return false;

    if (a.LayoutBefore != b.LayoutBefore || a.LayoutAfter != b.LayoutAfter)
        return false;

    // Must have the same flags
    if (a.Flags != b.Flags)
        return false;

    // Must have the same plane
    if (a.Subresources.FirstPlane != b.Subresources.FirstPlane || a.Subresources.NumPlanes != b.Subresources.NumPlanes)
        return false;

    // Check if subresources are contiguous
    // Case 1: Adjacent mips in the same array slice
    if (a.Subresources.FirstArraySlice == b.Subresources.FirstArraySlice &&
        a.Subresources.NumArraySlices == b.Subresources.NumArraySlices && a.Subresources.NumArraySlices == 1)
    {
        // Check if mips are adjacent
        u32 aLastMip = a.Subresources.IndexOrFirstMipLevel + a.Subresources.NumMipLevels;
        if (aLastMip == b.Subresources.IndexOrFirstMipLevel)
            return true;
    }

    // Case 2: Adjacent array slices with the same mip range
    if (a.Subresources.IndexOrFirstMipLevel == b.Subresources.IndexOrFirstMipLevel &&
        a.Subresources.NumMipLevels == b.Subresources.NumMipLevels)
    {
        // Check if array slices are adjacent
        u32 aLastSlice = a.Subresources.FirstArraySlice + a.Subresources.NumArraySlices;
        if (aLastSlice == b.Subresources.FirstArraySlice)
            return true;
    }

    return false;
}

static D3D12_TEXTURE_BARRIER MergeBarriers(const D3D12_TEXTURE_BARRIER& a, const D3D12_TEXTURE_BARRIER& b)
{
    D3D12_TEXTURE_BARRIER merged = a;

    // Merge adjacent mips (same array slice)
    if (a.Subresources.FirstArraySlice == b.Subresources.FirstArraySlice &&
        a.Subresources.NumArraySlices == b.Subresources.NumArraySlices)
    {
        // Determine which comes first
        if (a.Subresources.IndexOrFirstMipLevel < b.Subresources.IndexOrFirstMipLevel)
        {
            merged.Subresources.IndexOrFirstMipLevel = a.Subresources.IndexOrFirstMipLevel;
        }
        else
        {
            merged.Subresources.IndexOrFirstMipLevel = b.Subresources.IndexOrFirstMipLevel;
        }
        merged.Subresources.NumMipLevels = a.Subresources.NumMipLevels + b.Subresources.NumMipLevels;
    }
    // Merge adjacent array slices (same mip range)
    else if (a.Subresources.IndexOrFirstMipLevel == b.Subresources.IndexOrFirstMipLevel &&
             a.Subresources.NumMipLevels == b.Subresources.NumMipLevels)
    {
        // Determine which comes first
        if (a.Subresources.FirstArraySlice < b.Subresources.FirstArraySlice)
        {
            merged.Subresources.FirstArraySlice = a.Subresources.FirstArraySlice;
        }
        else
        {
            merged.Subresources.FirstArraySlice = b.Subresources.FirstArraySlice;
        }
        merged.Subresources.NumArraySlices = a.Subresources.NumArraySlices + b.Subresources.NumArraySlices;
    }

    return merged;
}

struct DX12BufferTextureCopyDesc
{
    D3D12_TEXTURE_COPY_LOCATION bufferLoc;
    D3D12_TEXTURE_COPY_LOCATION textureLoc;
    D3D12_BOX box;
};

static DX12BufferTextureCopyDesc GetCopyLocationsFromCopyDesc(const ComPtr<DX12Device>& device,
                                                              RHIBuffer& buffer,
                                                              RHITexture& texture,
                                                              const BufferTextureCopyDesc& desc)
{
    VEX_CHECK(IsAligned<u64>(desc.bufferRegion.offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT),
              "Source offset should be aligned to 512 bytes!");

    D3D12_TEXTURE_COPY_LOCATION bufferLoc = {};
    bufferLoc.pResource = buffer.GetRawBuffer();
    bufferLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
#if DX_DIRECT_CALLS
    D3D12_RESOURCE_DESC textureDesc = texture.GetRawTexture()->GetDesc();
#else
    D3D12_RESOURCE_DESC textureDesc;
    texture.GetRawTexture()->GetDesc(&textureDesc);
#endif
    if (textureDesc.Flags & D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT)
    {
        // Tight alignment means we have to force the alignment field to 0.
        textureDesc.Alignment = 0;
    }
    const u32 subresourceIndex =
        desc.textureRegion.subresource.startSlice * texture.GetDesc().mips + desc.textureRegion.subresource.startMip;
    device->GetCopyableFootprints(&textureDesc,
                                  subresourceIndex,
                                  1,
                                  desc.bufferRegion.offset,
                                  &bufferLoc.PlacedFootprint,
                                  nullptr,
                                  nullptr,
                                  nullptr);

    D3D12_TEXTURE_COPY_LOCATION textureLoc = {};
    textureLoc.pResource = texture.GetRawTexture();
    textureLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    textureLoc.SubresourceIndex = subresourceIndex;

    D3D12_BOX box = {};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = desc.textureRegion.extent.GetWidth(texture.GetDesc(), desc.textureRegion.subresource.startMip);
    box.bottom = desc.textureRegion.extent.GetHeight(texture.GetDesc(), desc.textureRegion.subresource.startMip);
    box.back = desc.textureRegion.extent.GetDepth(texture.GetDesc(), desc.textureRegion.subresource.startMip);

    return { bufferLoc, textureLoc, box };
}

} // namespace CommandList_Internal

DX12CommandList::DX12CommandList(const ComPtr<DX12Device>& device, QueueType type)
    : RHICommandListBase{ type }
    , device{ device }
{
    D3D12_COMMAND_LIST_TYPE d3dType;
    switch (type)
    {
    case QueueType::Copy:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY;
        break;

    case QueueType::Compute:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;

    case QueueType::Graphics:
        d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;

    default:
        VEX_LOG(Fatal, "Invalid command queue type passed to command list creation.");
    }

    // Create CommandList1 creates the command list closed by default.
    chk << device->CreateCommandList1(0, d3dType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));
    chk << device->CreateCommandAllocator(d3dType, IID_PPV_ARGS(&commandAllocator));
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
    case QueueType::Graphics:
        commandList->SetGraphicsRootSignature(globalRootSignature);
    case QueueType::Compute:
        commandList->SetComputeRootSignature(globalRootSignature);
    case QueueType::Copy:
    default:
        break;
    }

    std::span<const byte> localConstantsData = layout.GetLocalConstantsData();
    if (localConstantsData.empty())
    {
        return;
    }

    switch (type)
    {
    case QueueType::Graphics:
        // Set local constants (in first slot of root signature).
        commandList->SetGraphicsRoot32BitConstants(0,
                                                   static_cast<u32>(DivRoundUp(localConstantsData.size(), sizeof(u32))),
                                                   localConstantsData.data(),
                                                   0);
    case QueueType::Compute:
        // Set local constants (in first slot of root signature).
        commandList->SetComputeRoot32BitConstants(0,
                                                  static_cast<u32>(DivRoundUp(localConstantsData.size(), sizeof(u32))),
                                                  localConstantsData.data(),
                                                  0);
    case QueueType::Copy:
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
    DX12TextureView dxTextureView{ binding.binding };
    const u32 maxMip = dxTextureView.subresource.startMip + dxTextureView.subresource.mipCount;
    // We'll be creating a RTV/DSV view per-mip.
    dxTextureView.subresource.mipCount = 1;

    // Clearing in DX12 allows for multiple slices to be cleared, however you cannot clear multiple mips with one
    // call.
    // Instead we iterate on the mips passed in by the user.
    if (usage == TextureUsage::RenderTarget)
    {
        dxTextureView.usage = TextureUsage::RenderTarget;
        for (u32 mip = dxTextureView.subresource.startMip; mip < maxMip; ++mip)
        {
            dxTextureView.subresource.startMip = mip;
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
        dxTextureView.usage = TextureUsage::DepthStencil;
        for (u32 mip = dxTextureView.subresource.startMip; mip < maxMip; ++mip)
        {
            dxTextureView.subresource.startMip = mip;
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
                       binding.texture->GetDesc().name);

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
                binding.texture->GetDesc().name);
    }
}

void DX12CommandList::Barrier(std::span<const RHIBufferBarrier> bufferBarriers,
                              std::span<const RHITextureBarrier> textureBarriers)
{
    std::vector<D3D12_BUFFER_BARRIER> dx12BufferBarriers;
    dx12BufferBarriers.reserve(bufferBarriers.size());
    for (const auto& bufferBarrier : bufferBarriers)
    {
        D3D12_BUFFER_BARRIER dx12Barrier = {};
        dx12Barrier.SyncBefore = RHIBarrierSyncToDX12(bufferBarrier.buffer->GetLastSync());
        dx12Barrier.SyncAfter = RHIBarrierSyncToDX12(bufferBarrier.dstSync);
        dx12Barrier.AccessBefore = RHIBarrierAccessToDX12(bufferBarrier.buffer->GetLastAccess());
        dx12Barrier.AccessAfter = RHIBarrierAccessToDX12(bufferBarrier.dstAccess);
        dx12Barrier.pResource = bufferBarrier.buffer->GetRawBuffer();
        // Buffer range - for now, barrier entire buffer.
        dx12Barrier.Offset = 0;
        dx12Barrier.Size = std::numeric_limits<u64>::max();
        dx12BufferBarriers.push_back(std::move(dx12Barrier));

        // Update last sync and access.
        bufferBarrier.buffer->SetLastSync(bufferBarrier.dstSync);
        bufferBarrier.buffer->SetLastAccess(bufferBarrier.dstAccess);
    }

    std::vector<D3D12_TEXTURE_BARRIER> dx12TextureBarriers;
    dx12TextureBarriers.reserve(textureBarriers.size());
    for (const auto& textureBarrier : textureBarriers)
    {
        // Check if we can use the fast-path.
        const bool isSubresourceFullResource = textureBarrier.subresource == TextureSubresource{};
        if (isSubresourceFullResource && textureBarrier.texture->IsLastBarrierStateUniform())
        {
            D3D12_TEXTURE_BARRIER dx12Barrier = {};
            dx12Barrier.SyncBefore = RHIBarrierSyncToDX12(textureBarrier.texture->GetLastSync());
            dx12Barrier.SyncAfter = RHIBarrierSyncToDX12(textureBarrier.dstSync);
            dx12Barrier.AccessBefore = RHIBarrierAccessToDX12(textureBarrier.texture->GetLastAccess());
            dx12Barrier.AccessAfter = RHIBarrierAccessToDX12(textureBarrier.dstAccess);
            dx12Barrier.LayoutBefore = RHITextureLayoutToDX12(textureBarrier.texture->GetLastLayout());
            dx12Barrier.LayoutAfter = RHITextureLayoutToDX12(textureBarrier.dstLayout);
            dx12Barrier.pResource = textureBarrier.texture->GetRawTexture();

            // Copy command queues do not support the CopyDest stage.
            bool remapToCommon = false;
            if (type == QueueType::Copy && textureBarrier.dstLayout == RHITextureLayout::CopyDest)
            {
                dx12Barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
                remapToCommon = true;
            }

            if (dx12Barrier.AccessAfter & D3D12_BARRIER_ACCESS_NO_ACCESS)
            {
                dx12Barrier.SyncAfter = D3D12_BARRIER_SYNC_NONE;
            }

            // Handle binding subresource, to allow for a transition per-mip / per-slice.
            dx12Barrier.Subresources.IndexOrFirstMipLevel = textureBarrier.subresource.startMip;
            dx12Barrier.Subresources.NumMipLevels =
                textureBarrier.subresource.GetMipCount(textureBarrier.texture->GetDesc());
            dx12Barrier.Subresources.FirstArraySlice = textureBarrier.subresource.startSlice;
            dx12Barrier.Subresources.NumArraySlices =
                textureBarrier.subresource.GetSliceCount(textureBarrier.texture->GetDesc());
            dx12Barrier.Subresources.FirstPlane = 0;
            dx12Barrier.Subresources.NumPlanes = 1; // Most textures have 1 plane
            dx12Barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
            dx12TextureBarriers.push_back(std::move(dx12Barrier));

            // Update last barrier state for the resource.
            textureBarrier.texture->SetLastBarrierState(
                textureBarrier.dstSync,
                textureBarrier.dstAccess,
                remapToCommon ? RHITextureLayout::Common : textureBarrier.dstLayout);
        }
        else
        {
            // Ensures the texture uses non-uniform last barrier states.
            textureBarrier.texture->EnsureLastBarrierStateNonUniform();

            // We have to iterate on the barrier's subresource ranges.
            for (u16 mip = textureBarrier.subresource.startMip;
                 mip < textureBarrier.subresource.startMip +
                           textureBarrier.subresource.GetMipCount(textureBarrier.texture->GetDesc());
                 ++mip)
            {
                for (u32 slice = textureBarrier.subresource.startSlice;
                     slice < textureBarrier.subresource.startSlice +
                                 textureBarrier.subresource.GetSliceCount(textureBarrier.texture->GetDesc());
                     ++slice)
                {
                    D3D12_TEXTURE_BARRIER dx12Barrier = {};
                    dx12Barrier.SyncBefore =
                        RHIBarrierSyncToDX12(textureBarrier.texture->GetLastSyncForSubresource(mip, slice));
                    dx12Barrier.SyncAfter = RHIBarrierSyncToDX12(textureBarrier.dstSync);
                    dx12Barrier.AccessBefore =
                        RHIBarrierAccessToDX12(textureBarrier.texture->GetLastAccessForSubresource(mip, slice));
                    dx12Barrier.AccessAfter = RHIBarrierAccessToDX12(textureBarrier.dstAccess);
                    dx12Barrier.LayoutBefore =
                        RHITextureLayoutToDX12(textureBarrier.texture->GetLastLayoutForSubresource(mip, slice));
                    dx12Barrier.LayoutAfter = RHITextureLayoutToDX12(textureBarrier.dstLayout);
                    dx12Barrier.pResource = textureBarrier.texture->GetRawTexture();

                    // Copy command queues do not support the CopyDest stage.
                    bool remapToCommon = false;
                    if (type == QueueType::Copy && textureBarrier.dstLayout == RHITextureLayout::CopyDest)
                    {
                        dx12Barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
                        remapToCommon = true;
                    }

                    if (dx12Barrier.AccessAfter & D3D12_BARRIER_ACCESS_NO_ACCESS)
                    {
                        dx12Barrier.SyncAfter = D3D12_BARRIER_SYNC_NONE;
                    }

                    // Handle binding subresource, to allow for a transition per-mip / per-slice.
                    dx12Barrier.Subresources.IndexOrFirstMipLevel = mip;
                    dx12Barrier.Subresources.NumMipLevels = 1;
                    dx12Barrier.Subresources.FirstArraySlice = slice;
                    dx12Barrier.Subresources.NumArraySlices = 1;
                    dx12Barrier.Subresources.FirstPlane = 0;
                    dx12Barrier.Subresources.NumPlanes = 1; // Most textures have 1 plane
                    dx12Barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
                    dx12TextureBarriers.push_back(std::move(dx12Barrier));

                    if (!isSubresourceFullResource)
                    {
                        // Update last barrier state for the subresource.
                        textureBarrier.texture->SetLastBarrierStateForSubresource(
                            textureBarrier.dstSync,
                            textureBarrier.dstAccess,
                            remapToCommon ? RHITextureLayout::Common : textureBarrier.dstLayout,
                            mip,
                            slice);
                    }
                }
            }

            // If the dst barrier is constant across the entire resource, we can just revert to uniform barriers.
            if (isSubresourceFullResource)
            {
                const bool remapToCommon =
                    type == QueueType::Copy && textureBarrier.dstLayout == RHITextureLayout::CopyDest;
                // Update last barrier state for the resource.
                textureBarrier.texture->SetLastBarrierState(
                    textureBarrier.dstSync,
                    textureBarrier.dstAccess,
                    remapToCommon ? RHITextureLayout::Common : textureBarrier.dstLayout);
            }
        }
    }

    // Now we perform a compaction pass on texture barriers to catch neighboring barriers with the same src AND dst
    // values.
    std::vector<D3D12_TEXTURE_BARRIER> compactedDX12TextureBarriers;
    compactedDX12TextureBarriers.reserve(dx12TextureBarriers.size());
    for (u32 i = 0; i < dx12TextureBarriers.size(); ++i)
    {
        D3D12_TEXTURE_BARRIER current = dx12TextureBarriers[i];
        // Keep merging while possible.
        while (i + 1 < dx12TextureBarriers.size() &&
               CommandList_Internal::CanMergeBarriers(current, dx12TextureBarriers[i + 1]))
        {
            current = CommandList_Internal::MergeBarriers(current, dx12TextureBarriers[i + 1]);
            ++i; // Skip the merged barrier
        }
        compactedDX12TextureBarriers.push_back(current);
    }

    // Take our barriers and now insert them into "groups" to be sent to the command list.
    std::vector<D3D12_BARRIER_GROUP> barrierGroups;
    barrierGroups.reserve(compactedDX12TextureBarriers.size() + dx12BufferBarriers.size());

    if (!compactedDX12TextureBarriers.empty())
    {
        D3D12_BARRIER_GROUP textureGroup = {};
        textureGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
        textureGroup.NumBarriers = static_cast<UINT>(compactedDX12TextureBarriers.size());
        textureGroup.pTextureBarriers = compactedDX12TextureBarriers.data();
        barrierGroups.push_back(textureGroup);
    }

    if (!dx12BufferBarriers.empty())
    {
        D3D12_BARRIER_GROUP bufferGroup = {};
        bufferGroup.Type = D3D12_BARRIER_TYPE_BUFFER;
        bufferGroup.NumBarriers = static_cast<UINT>(dx12BufferBarriers.size());
        bufferGroup.pBufferBarriers = dx12BufferBarriers.data();
        barrierGroups.push_back(bufferGroup);
    }

    VEX_ASSERT(!barrierGroups.empty(), "BarrierGroups cannot be empty...");
    commandList->Barrier(barrierGroups.size(), barrierGroups.data());
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
    if (type == QueueType::Graphics)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE* dsvHandlePtr = dsvHandle.has_value() ? &dsvHandle.value() : nullptr;
        commandList->OMSetRenderTargets(static_cast<u32>(rtvHandles.size()), rtvHandles.data(), false, dsvHandlePtr);
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
    if (type != QueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    commandList->DrawInstanced(vertexCount, instanceCount, vertexOffset, instanceOffset);
}

void DX12CommandList::DrawIndexed(
    u32 indexCount, u32 instanceCount, u32 indexOffset, u32 vertexOffset, u32 instanceOffset)
{
    if (type != QueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    commandList->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
}

void DX12CommandList::SetVertexBuffers(u32 startSlot, std::span<RHIBufferBinding> vertexBuffers)
{
    if (type != QueueType::Graphics)
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
    if (type != QueueType::Graphics)
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
    case QueueType::Graphics:
    case QueueType::Compute:
        commandList->Dispatch(groupCount[0], groupCount[1], groupCount[2]);
        break;
    case QueueType::Copy:
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
    case QueueType::Graphics:
    case QueueType::Compute:
        commandList->DispatchRays(&rayDesc);
    case QueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::GenerateMips(RHITexture& texture, u16 sourceMip)
{
    VEX_CHECK(false, "DX12 does not support built-in mip generation.");
}

void DX12CommandList::Copy(RHITexture& src, RHITexture& dst)
{
    VEX_ASSERT(src.GetDesc().width == dst.GetDesc().width && src.GetDesc().height == dst.GetDesc().height &&
                   src.GetDesc().depthOrSliceCount == dst.GetDesc().depthOrSliceCount &&
                   src.GetDesc().mips == dst.GetDesc().mips && src.GetDesc().format == dst.GetDesc().format,
               "The two textures must be compatible in order to Copy to be useable.");
    commandList->CopyResource(dst.GetRawTexture(), src.GetRawTexture());
}

void DX12CommandList::Copy(RHITexture& src, RHITexture& dst, std::span<const TextureCopyDesc> textureCopyDescs)
{
    ID3D12Resource* srcTexture = src.GetRawTexture();
    ID3D12Resource* dstTexture = dst.GetRawTexture();

    for (const TextureCopyDesc& copyDesc : textureCopyDescs)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = srcTexture;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex =
            copyDesc.srcRegion.subresource.startSlice * src.GetDesc().mips + copyDesc.srcRegion.subresource.startMip;

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = dstTexture;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex =
            copyDesc.dstRegion.subresource.startSlice * dst.GetDesc().mips + copyDesc.dstRegion.subresource.startMip;

        D3D12_BOX srcBox = {};
        srcBox.left = copyDesc.srcRegion.offset.x;
        srcBox.top = copyDesc.srcRegion.offset.y;
        srcBox.front = copyDesc.srcRegion.offset.z;
        srcBox.right = copyDesc.srcRegion.offset.x +
                       copyDesc.srcRegion.extent.GetWidth(src.GetDesc(), copyDesc.srcRegion.subresource.startMip);
        srcBox.bottom = copyDesc.srcRegion.offset.y +
                        copyDesc.srcRegion.extent.GetHeight(src.GetDesc(), copyDesc.srcRegion.subresource.startMip);
        srcBox.back = copyDesc.srcRegion.offset.z +
                      copyDesc.srcRegion.extent.GetDepth(src.GetDesc(), copyDesc.srcRegion.subresource.startMip);

        commandList->CopyTextureRegion(&dstLoc,
                                       copyDesc.dstRegion.offset.x,
                                       copyDesc.dstRegion.offset.y,
                                       copyDesc.dstRegion.offset.z,
                                       &srcLoc,
                                       &srcBox);
    }
}

void DX12CommandList::Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDescription)
{
    commandList->CopyBufferRegion(dst.GetRawBuffer(),
                                  bufferCopyDescription.dstOffset,
                                  src.GetRawBuffer(),
                                  bufferCopyDescription.srcOffset,
                                  bufferCopyDescription.GetByteSize(src.GetDesc()));
}

void DX12CommandList::Copy(RHIBuffer& src, RHITexture& dst, std::span<const BufferTextureCopyDesc> copyDescriptions)
{
    for (const BufferTextureCopyDesc& copyDesc : copyDescriptions)
    {
        auto locations = CommandList_Internal::GetCopyLocationsFromCopyDesc(device, src, dst, copyDesc);
        commandList->CopyTextureRegion(&locations.textureLoc,
                                       copyDesc.textureRegion.offset.x,
                                       copyDesc.textureRegion.offset.y,
                                       copyDesc.textureRegion.offset.z,
                                       &locations.bufferLoc,
                                       &locations.box);
    }
}

void DX12CommandList::Copy(RHITexture& src, RHIBuffer& dst, std::span<const BufferTextureCopyDesc> copyDescriptions)
{
    for (const BufferTextureCopyDesc& copyDesc : copyDescriptions)
    {
        auto locations = CommandList_Internal::GetCopyLocationsFromCopyDesc(device, dst, src, copyDesc);
        commandList->CopyTextureRegion(&locations.bufferLoc,
                                       copyDesc.textureRegion.offset.x,
                                       copyDesc.textureRegion.offset.y,
                                       copyDesc.textureRegion.offset.z,
                                       &locations.textureLoc,
                                       &locations.box);
    }
}

RHIScopedGPUEvent DX12CommandList::CreateScopedMarker(const char* label, std::array<float, 3> labelColor)
{
    return { commandList, label, labelColor };
}

} // namespace vex::dx12