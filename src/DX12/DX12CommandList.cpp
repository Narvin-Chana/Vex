#include "DX12CommandList.h"

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/RHI/RHIBindings.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12PipelineState.h>
#include <DX12/DX12ResourceLayout.h>
#include <DX12/DX12States.h>
#include <DX12/DX12Texture.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

namespace CommandList_Internal
{

static TextureViewType GetTextureViewType(const ResourceBinding& binding)
{
    switch (binding.texture.description.type)
    {
    case TextureType::Texture2D:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::Texture2DArray
                                                                  : TextureViewType::Texture2D;
    case TextureType::TextureCube:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::TextureCubeArray
                                                                  : TextureViewType::TextureCube;
    case TextureType::Texture3D:
        return TextureViewType::Texture3D;
    default:
        VEX_LOG(Fatal, "Unrecognized texture type...");
    }
    std::unreachable();
}

static DXGI_FORMAT GetTextureFormat(const ResourceBinding& binding)
{
    if (!IsFormatSRGB(binding.texture.description.format) &&
        !FormatHasSRGBEquivalent(binding.texture.description.format))
    {
        return DXGI_FORMAT_UNKNOWN;
    }

    DXGI_FORMAT nativeFormat = TextureFormatToDXGI(binding.texture.description.format);

    if (binding.textureFlags & TextureBinding::SRGB)
    {
        return GetSRGBFormatForSRGBCompatibleDX12Format(nativeFormat);
    }

    return nativeFormat;
}

} // namespace CommandList_Internal

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
    const auto& dxResourceLayout = reinterpret_cast<const DX12ResourceLayout&>(layout);
    u32 rootSignatureDWORDCount = dxResourceLayout.GetMaxLocalConstantSize() / sizeof(DWORD);

    u32 localConstantsByteSize = 0;
    // Compute total size of constants (and make sure the constants fit in local constants).
    for (const auto& binding : constants)
    {
        localConstantsByteSize += static_cast<u32>(binding.size);
    }

    if (localConstantsByteSize > dxResourceLayout.GetMaxLocalConstantSize())
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

    // Data is stored in the form of 32bit chunks, so there is still a chance our local data is too fat to fit in root
    // constant storage.
    u32 finalByteSize = DivideAndRoundUp(localConstantsByteSize, sizeof(DWORD));
    if (finalByteSize > dxResourceLayout.GetMaxLocalConstantSize())
    {
        VEX_LOG(Fatal,
                "Unable to bind local constants, you have surpassed the limit DX12 allows for in root signatures.");
    }

    if (finalByteSize == 0)
    {
        return;
    }

    // Padding to fill out the unused local constants space.
    dataToUpload.resize(rootSignatureDWORDCount);

    switch (type)
    {
    case CommandQueueType::Graphics:
        commandList->SetGraphicsRoot32BitConstants(0, rootSignatureDWORDCount, dataToUpload.data(), 0);
    case CommandQueueType::Compute:
        commandList->SetComputeRoot32BitConstants(0, rootSignatureDWORDCount, dataToUpload.data(), 0);
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
    using namespace CommandList_Internal;

    const auto& dxResourceLayout = reinterpret_cast<const DX12ResourceLayout&>(layout);
    auto& dxDescriptorPool = reinterpret_cast<DX12DescriptorPool&>(descriptorPool);

    std::vector<BindlessHandle> bindlessHandles;
    // Allocate for worst-case scenario.
    bindlessHandles.reserve(textures.size() + buffers.size());

    for (auto& [binding, usage, rhiTexture] : textures)
    {
        auto dxTexture = reinterpret_cast<DX12Texture*>(rhiTexture);

        if (usage == ResourceUsage::Read || usage == ResourceUsage::UnorderedAccess)
        {
            bindlessHandles.push_back(dxTexture->GetOrCreateBindlessView(
                device,
                DX12TextureView{
                    .type = usage,
                    .dimension = GetTextureViewType(binding),
                    .format = GetTextureFormat(binding),
                    .mipBias = binding.mipBias,
                    .mipCount = (binding.mipCount == 0) ? rhiTexture->GetDescription().mips : binding.mipCount,
                    .startSlice = binding.startSlice,
                    .sliceCount =
                        (binding.sliceCount == 0) ? rhiTexture->GetDescription().depthOrArraySize : binding.sliceCount,

                },
                dxDescriptorPool));
        }
        else // if (usage == ResourceUsage::RenderTarget || usage == ResourceUsage::DepthStencil)
        {
            // TODO: We can just bind these directly to the command list using their D3D12_CPU_DESCRIPTOR_HANDLE!
            // Currently not handled with the compute-only pipeline (since it has no need for RTV/DSVs).
        }
    }

    for (auto& [binding, usage, rhiBuffer] : buffers)
    {
        // TODO: implement buffers!
    }

    // Now we can bind the bindless textures as constants in our root constants!
    // TODO: figure out how this interacts with local root constants, there should be a way to get the first slot we can
    // write bindless indices to (that is after local constants). For now we just default to slot 0, and suppose that no
    // constants exist (just to get our Hello Triangle working).
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

void DX12CommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool)
{
    commandList->SetDescriptorHeaps(
        1,
        reinterpret_cast<DX12DescriptorPool&>(descriptorPool).gpuHeap.GetRawDescriptorHeap().GetAddressOf());
}

void DX12CommandList::Transition(RHITexture& texture, RHITextureState::Flags newState)
{
    // Nothing to do if the states are already equal.
    if (texture.GetCurrentState() == newState)
    {
        return;
    }

    DX12Texture& dxTexture = reinterpret_cast<DX12Texture&>(texture);
    D3D12_RESOURCE_STATES currentDX12State = RHITextureStateToDX12State(texture.GetCurrentState());
    D3D12_RESOURCE_STATES newDX12State = RHITextureStateToDX12State(newState);
    D3D12_RESOURCE_BARRIER resourceBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(dxTexture.GetRawTexture(), currentDX12State, newDX12State);

    texture.SetCurrentState(newState);

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
        if (newDX12State == currentDX12State)
        {
            continue;
        }
        DX12Texture& dxTexture = reinterpret_cast<DX12Texture&>(texture);
        D3D12_RESOURCE_BARRIER resourceBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(dxTexture.GetRawTexture(), currentDX12State, newDX12State);

        texture.SetCurrentState(newState);

        transitionBarriers.push_back(std::move(resourceBarrier));
    }

    if (!transitionBarriers.empty())
    {
        commandList->ResourceBarrier(transitionBarriers.size(), transitionBarriers.data());
    }
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

CommandQueueType DX12CommandList::GetType() const
{
    return type;
}

} // namespace vex::dx12