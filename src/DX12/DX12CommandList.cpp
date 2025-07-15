#include "DX12CommandList.h"

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/RHI/RHIBindings.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12GraphicsPipeline.h>
#include <DX12/DX12PipelineState.h>
#include <DX12/DX12ResourceLayout.h>
#include <DX12/DX12States.h>
#include <DX12/DX12Texture.h>
#include <DX12/HRChecker.h>

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
}
void DX12CommandList::SetLayoutLocalConstants(const RHIResourceLayout& layout,
                                              std::span<const ConstantBinding> constants)
{
    if (constants.empty())
    {
        return;
    }

    auto constantData = ConstantBinding::ConcatConstantBindings(constants, layout.GetMaxLocalConstantSize());

    auto DWORDCount = constantData.size() / sizeof(DWORD);

    switch (type)
    {
    case CommandQueueType::Graphics:
        commandList->SetGraphicsRoot32BitConstants(0, DWORDCount, constantData.data(), 0);
    case CommandQueueType::Compute:
        commandList->SetComputeRoot32BitConstants(0, DWORDCount, constantData.data(), 0);
    case CommandQueueType::Copy:
    default:
        break;
    }
}

void DX12CommandList::SetLayoutResources(const RHIResourceLayout& layout,
                                         std::span<RHITextureBinding> textures,
                                         std::span<RHIBufferBinding> buffers,
                                         RHIDescriptorPool& descriptorPool)
{
    auto& dxDescriptorPool = reinterpret_cast<DX12DescriptorPool&>(descriptorPool);

    std::vector<BindlessHandle> bindlessHandles;
    // Allocate for worst-case scenario.
    bindlessHandles.reserve(textures.size() + buffers.size());

    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    rtvHandles.reserve(8);

    std::optional<CD3DX12_CPU_DESCRIPTOR_HANDLE> dsvHandle;

    for (auto& [binding, usage, rhiTexture] : textures)
    {
        auto dxTexture = reinterpret_cast<DX12Texture&>(*rhiTexture);
        DX12TextureView dxTextureView{ binding, rhiTexture->GetDescription(), usage };
        if (usage & ResourceUsage::Read || usage & ResourceUsage::UnorderedAccess)
        {
            bindlessHandles.push_back(dxTexture.GetOrCreateBindlessView(device, dxTextureView, dxDescriptorPool));
        }
        else if (usage & ResourceUsage::RenderTarget)
        {
            rtvHandles.emplace_back(dxTexture.GetOrCreateRTVDSVView(device, dxTextureView));
        }
        else if (usage & ResourceUsage::DepthStencil)
        {
            dsvHandle = dxTexture.GetOrCreateRTVDSVView(device, dxTextureView);
        }
    }

    for (auto& [binding, usage, rhiBuffer] : buffers)
    {
        VEX_NOT_YET_IMPLEMENTED();
    }

    // Now we can bind the bindless textures as constants in our root constants!
    // TODO: figure out how this interacts with local root constants, there should be a way to get the first slot we can
    // write bindless indices to (that is after local constants). For now we just default to slot 0, and suppose that no
    // constants exist (just to get our Hello Triangle working).
    if (!bindlessHandles.empty())
    {
        switch (type)
        {
        case CommandQueueType::Graphics:
            commandList->SetGraphicsRoot32BitConstants(0,
                                                       static_cast<u32>(bindlessHandles.size()),
                                                       bindlessHandles.data(),
                                                       0);
        case CommandQueueType::Compute:
            commandList->SetComputeRoot32BitConstants(0,
                                                      static_cast<u32>(bindlessHandles.size()),
                                                      bindlessHandles.data(),
                                                      0);
        case CommandQueueType::Copy:
        default:
            break;
        }
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

void DX12CommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    commandList->SetDescriptorHeaps(
        1,
        reinterpret_cast<DX12DescriptorPool&>(descriptorPool).gpuHeap.GetRawDescriptorHeap().GetAddressOf());
}

void DX12CommandList::SetInputAssembly(InputAssembly inputAssembly)
{
    commandList->IASetPrimitiveTopology(GraphicsPipeline::GetDX12PrimitiveTopologyFromInputAssembly(inputAssembly));
}

void DX12CommandList::ClearTexture(RHITexture& rhiTexture,
                                   const ResourceBinding& clearBinding,
                                   const TextureClearValue& clearValue)
{
    const TextureDescription& desc = rhiTexture.GetDescription();

    // Clearing in DX12 allows for multiple slices to be cleared, however you cannot clear multiple mips with one
    // call.
    // Instead we iterate on the mips passed in by the user.

    if (desc.usage & ResourceUsage::RenderTarget)
    {
        DX12TextureView dxTextureView{ clearBinding, desc, ResourceUsage::RenderTarget };
        dxTextureView.mipCount = 1;

        for (u32 mip = clearBinding.mipBias;
             mip < ((clearBinding.mipCount == 0) ? 1 : (clearBinding.mipBias + clearBinding.mipCount));
             ++mip)
        {
            dxTextureView.mipBias = mip;
            VEX_ASSERT(clearValue.flags & TextureClear::ClearColor,
                       "Clearing the color requires the TextureClear::ClearColor flag for texture: {}.",
                       desc.name);
            commandList->ClearRenderTargetView(
                reinterpret_cast<DX12Texture&>(rhiTexture).GetOrCreateRTVDSVView(device, dxTextureView),
                clearValue.color,
                0,
                nullptr);
        }
    }
    else if (desc.usage & ResourceUsage::DepthStencil)
    {
        DX12TextureView dxTextureView{ clearBinding, desc, ResourceUsage::DepthStencil };
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
                       desc.name);

            commandList->ClearDepthStencilView(
                reinterpret_cast<DX12Texture&>(rhiTexture).GetOrCreateRTVDSVView(device, dxTextureView),
                clearFlags,
                clearValue.depth,
                clearValue.stencil,
                0,
                nullptr);
        }
    }
    else
    {
        VEX_LOG(Fatal, "The passed in texture does not support the usage required to be cleared: {}.", desc.name);
    }
}

void DX12CommandList::Transition(RHITexture& texture, RHITextureState::Flags newState)
{
    DX12Texture& dxTexture = reinterpret_cast<DX12Texture&>(texture);
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
        CD3DX12_RESOURCE_BARRIER::Transition(dxTexture.GetRawTexture(), currentDX12State, newDX12State);

    commandList->ResourceBarrier(1, &resourceBarrier);
}

void DX12CommandList::Transition(RHIBuffer& buffer, RHIBufferState::Flags newState)
{
    VEX_NOT_YET_IMPLEMENTED();
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
        DX12Texture& dxTexture = reinterpret_cast<DX12Texture&>(texture);
        D3D12_RESOURCE_BARRIER resourceBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(dxTexture.GetRawTexture(), currentDX12State, newDX12State);
        transitionBarriers.push_back(std::move(resourceBarrier));
    }

    if (!transitionBarriers.empty())
    {
        commandList->ResourceBarrier(transitionBarriers.size(), transitionBarriers.data());
    }
}

void DX12CommandList::Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void DX12CommandList::Draw(u32 vertexCount)
{
    if (type != CommandQueueType::Graphics)
    {
        VEX_LOG(Fatal, "Cannot use draw calls with a non-graphics command queue.");
    }

    commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void DX12CommandList::Dispatch(const std::array<u32, 3>& groupCount)
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
}

void DX12CommandList::Copy(RHITexture& src, RHITexture& dst)
{
    VEX_ASSERT(src.GetDescription().width == dst.GetDescription().width &&
                   src.GetDescription().height == dst.GetDescription().height &&
                   src.GetDescription().depthOrArraySize == dst.GetDescription().depthOrArraySize &&
                   src.GetDescription().mips == dst.GetDescription().mips &&
                   src.GetDescription().format == dst.GetDescription().format,
               "The two textures must be compatible in order to Copy to be useable.");
    ID3D12Resource* srcNative = reinterpret_cast<DX12Texture&>(src).GetRawTexture();
    ID3D12Resource* dstNative = reinterpret_cast<DX12Texture&>(dst).GetRawTexture();
    commandList->CopyResource(dstNative, srcNative);
}

void DX12CommandList::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    VEX_NOT_YET_IMPLEMENTED();
}

CommandQueueType DX12CommandList::GetType() const
{
    return type;
}

} // namespace vex::dx12