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

struct DX12BufferTextureCopyDesc
{
    D3D12_TEXTURE_COPY_LOCATION bufferLoc;
    D3D12_TEXTURE_COPY_LOCATION textureLoc;
    D3D12_BOX box;
};

DX12BufferTextureCopyDesc GetCopyLocationsFromCopyDesc(RHIBuffer& buffer,
                                                       RHITexture& texture,
                                                       const BufferTextureCopyDescription& desc)
{
    float formatPixelByteSize = TextureUtil::GetPixelByteSizeFromFormat(texture.GetDescription().format);

    const u32 alignedRowPitch =
        AlignUp<u32>(desc.extent.width * formatPixelByteSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    VEX_CHECK(IsAligned<u64>(desc.bufferSubresource.offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT),
              "Source offset should be aligned to 512 bytes!");

    D3D12_TEXTURE_COPY_LOCATION bufferLoc = {};
    bufferLoc.pResource = buffer.GetRawBuffer();
    bufferLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = bufferLoc.PlacedFootprint;
    footprint.Offset = desc.bufferSubresource.offset;
    footprint.Footprint.Format = TextureFormatToDXGI(texture.GetDescription().format);
    footprint.Footprint.Width = desc.extent.width;
    footprint.Footprint.Height = desc.extent.height;
    footprint.Footprint.Depth = desc.extent.depth;
    footprint.Footprint.RowPitch = alignedRowPitch;

    D3D12_TEXTURE_COPY_LOCATION textureLoc = {};
    textureLoc.pResource = texture.GetRawTexture();
    textureLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    textureLoc.SubresourceIndex =
        desc.textureSubresource.startSlice * texture.GetDescription().mips + desc.textureSubresource.mip;

    D3D12_BOX box = {};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = desc.extent.width;
    box.bottom = desc.extent.height;
    box.back = desc.extent.depth;

    return { bufferLoc, textureLoc, box };
}

} // namespace CommandList_Internal

DX12CommandList::DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type)
    : RHICommandListBase{ type }
    , device{ device }
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

    std::span<const byte> localConstantsData = layout.GetLocalConstantsData();
    if (localConstantsData.empty())
    {
        return;
    }

    switch (type)
    {
    case CommandQueueType::Graphics:
        // Set local constants (in first slot of root signature).
        commandList->SetGraphicsRoot32BitConstants(0,
                                                   static_cast<u32>(DivRoundUp(localConstantsData.size(), sizeof(u32))),
                                                   localConstantsData.data(),
                                                   0);
    case CommandQueueType::Compute:
        // Set local constants (in first slot of root signature).
        commandList->SetComputeRoot32BitConstants(0,
                                                  static_cast<u32>(DivRoundUp(localConstantsData.size(), sizeof(u32))),
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

void DX12CommandList::Barrier(std::span<const RHIBufferBarrier> bufferBarriers,
                              std::span<const RHITextureBarrier> textureBarriers)
{
    std::vector<D3D12_BUFFER_BARRIER> dx12BufferBarriers;
    dx12BufferBarriers.reserve(bufferBarriers.size());
    std::vector<D3D12_TEXTURE_BARRIER> dx12TextureBarriers;
    dx12TextureBarriers.reserve(textureBarriers.size());

    for (const auto& bufferBarrier : bufferBarriers)
    {
        D3D12_BUFFER_BARRIER dx12Barrier = {};
        dx12Barrier.SyncBefore = RHIBarrierSyncToDX12(bufferBarrier.srcSync);
        dx12Barrier.SyncAfter = RHIBarrierSyncToDX12(bufferBarrier.dstSync);
        dx12Barrier.AccessBefore = RHIBarrierAccessToDX12(bufferBarrier.srcAccess);
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
    for (const auto& textureBarrier : textureBarriers)
    {
        D3D12_TEXTURE_BARRIER dx12Barrier = {};
        dx12Barrier.SyncBefore = RHIBarrierSyncToDX12(textureBarrier.srcSync);
        dx12Barrier.SyncAfter = RHIBarrierSyncToDX12(textureBarrier.dstSync);
        dx12Barrier.AccessBefore = RHIBarrierAccessToDX12(textureBarrier.srcAccess);
        dx12Barrier.AccessAfter = RHIBarrierAccessToDX12(textureBarrier.dstAccess);
        dx12Barrier.LayoutBefore = RHITextureLayoutToDX12(textureBarrier.srcLayout);
        dx12Barrier.LayoutAfter = RHITextureLayoutToDX12(textureBarrier.dstLayout);
        dx12Barrier.pResource = textureBarrier.texture->GetRawTexture();

        // Copy command queues do not support the CopyDest stage.
        bool remapToCommon = false;
        if (type == CommandQueueType::Copy && textureBarrier.dstLayout == RHITextureLayout::CopyDest)
        {
            dx12Barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
            remapToCommon = true;
        }

        if (dx12Barrier.AccessAfter & D3D12_BARRIER_ACCESS_NO_ACCESS)
        {
            dx12Barrier.SyncAfter = D3D12_BARRIER_SYNC_NONE;
        }

        // Handle subresources - for now, barrier all subresources.
        // We might want to extend RHIBarrier to specify subresource ranges (...one day...maybe...).
        dx12Barrier.Subresources.IndexOrFirstMipLevel = 0;
        dx12Barrier.Subresources.NumMipLevels = textureBarrier.texture->GetDescription().mips;
        dx12Barrier.Subresources.FirstArraySlice = 0;
        dx12Barrier.Subresources.NumArraySlices = textureBarrier.texture->GetDescription().GetArraySize();
        dx12Barrier.Subresources.FirstPlane = 0;
        dx12Barrier.Subresources.NumPlanes = 1; // Most textures have 1 plane
        dx12Barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
        dx12TextureBarriers.push_back(std::move(dx12Barrier));

        // Update last sync, access and layout.
        textureBarrier.texture->SetLastSync(textureBarrier.dstSync);
        textureBarrier.texture->SetLastAccess(textureBarrier.dstAccess);
        textureBarrier.texture->SetLastLayout(remapToCommon ? RHITextureLayout::Common : textureBarrier.dstLayout);
    }

    // Take our barriers and now insert them into "groups" to be sent to the command list.
    std::vector<D3D12_BARRIER_GROUP> barrierGroups;
    barrierGroups.reserve(dx12TextureBarriers.size() + dx12BufferBarriers.size());

    if (!dx12TextureBarriers.empty())
    {
        D3D12_BARRIER_GROUP textureGroup = {};
        textureGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
        textureGroup.NumBarriers = static_cast<UINT>(dx12TextureBarriers.size());
        textureGroup.pTextureBarriers = dx12TextureBarriers.data();
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

    if (barrierGroups.empty())
    {
        return;
    }

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
    if (type == CommandQueueType::Graphics)
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

void DX12CommandList::GenerateMips(RHITexture& texture)
{
    VEX_CHECK(false, "DX12 does not currently support built-in mip generation.");
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

void DX12CommandList::Copy(RHITexture& src,
                           RHITexture& dst,
                           std::span<const TextureCopyDescription> textureCopyDescriptions)
{
    ID3D12Resource* srcTexture = src.GetRawTexture();
    ID3D12Resource* dstTexture = dst.GetRawTexture();

    for (const TextureCopyDescription& copyDesc : textureCopyDescriptions)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = srcTexture;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex =
            copyDesc.srcSubresource.startSlice * src.GetDescription().mips + copyDesc.srcSubresource.mip;

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = dstTexture;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex =
            copyDesc.dstSubresource.startSlice * dst.GetDescription().mips + copyDesc.dstSubresource.mip;

        D3D12_BOX srcBox = {};
        srcBox.left = copyDesc.srcSubresource.offset.width;
        srcBox.top = copyDesc.srcSubresource.offset.height;
        srcBox.front = copyDesc.srcSubresource.offset.depth;
        srcBox.right = copyDesc.srcSubresource.offset.width + copyDesc.extent.width;
        srcBox.bottom = copyDesc.srcSubresource.offset.height + copyDesc.extent.height;
        srcBox.back = copyDesc.srcSubresource.offset.depth + copyDesc.extent.depth;

        commandList->CopyTextureRegion(&dstLoc,
                                       copyDesc.dstSubresource.offset.width,
                                       copyDesc.dstSubresource.offset.height,
                                       copyDesc.dstSubresource.offset.depth,
                                       &srcLoc,
                                       &srcBox);
    }
}

void DX12CommandList::Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDescription& bufferCopyDescription)
{
    commandList->CopyBufferRegion(dst.GetRawBuffer(),
                                  bufferCopyDescription.dstOffset,
                                  src.GetRawBuffer(),
                                  bufferCopyDescription.srcOffset,
                                  bufferCopyDescription.size);
}

void DX12CommandList::Copy(RHIBuffer& src,
                           RHITexture& dst,
                           std::span<const BufferTextureCopyDescription> copyDescriptions)
{
    // TODO(https://trello.com/c/KEnbDLG6): this way of uploading makes it so that texture arrays are copied slice by
    // slice, when instead they could be copied mip element by mip. Would require considerable effort to fix and is only
    // a slight optimization so will probably be done later on.
    // Potentially use GetCopyableFootprints

    for (const BufferTextureCopyDescription& copyDesc : copyDescriptions)
    {
        auto locations = CommandList_Internal::GetCopyLocationsFromCopyDesc(src, dst, copyDesc);
        commandList->CopyTextureRegion(&locations.textureLoc,
                                       copyDesc.textureSubresource.offset.width,
                                       copyDesc.textureSubresource.offset.height,
                                       copyDesc.textureSubresource.offset.depth,
                                       &locations.bufferLoc,
                                       &locations.box);
    }
}

void DX12CommandList::Copy(RHITexture& src,
                           RHIBuffer& dst,
                           std::span<const BufferTextureCopyDescription> copyDescriptions)
{
    for (const BufferTextureCopyDescription& copyDesc : copyDescriptions)
    {
        auto locations = CommandList_Internal::GetCopyLocationsFromCopyDesc(dst, src, copyDesc);
        commandList->CopyTextureRegion(&locations.bufferLoc,
                                       copyDesc.textureSubresource.offset.width,
                                       copyDesc.textureSubresource.offset.height,
                                       copyDesc.textureSubresource.offset.depth,
                                       &locations.textureLoc,
                                       &locations.box);
    }
}

RHIScopedGPUEvent DX12CommandList::CreateScopedMarker(const char* label, std::array<float, 3> labelColor)
{
    return { commandList, label, labelColor };
}

} // namespace vex::dx12