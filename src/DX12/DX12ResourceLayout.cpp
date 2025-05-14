#include "DX12ResourceLayout.h"

#include <ranges>
#include <utility>

#include <Vex/Logger.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12ResourceLayout::DX12ResourceLayout(ComPtr<DX12Device>& device, const DX12FeatureChecker& featureChecker)
    : device(device)
    , featureChecker(featureChecker)
{
}

bool DX12ResourceLayout::ValidateGlobalConstant(const GlobalConstant& globalConstant) const
{
    if (!RHIResourceLayout::ValidateGlobalConstant(globalConstant))
    {
        return false;
    }

    // TODO: check size limits vs cbuffer limits

    return true;
}

u32 DX12ResourceLayout::GetMaxLocalConstantSize() const
{
    // Each global constant descriptor takes up 2 DWORDs in the root signature (as root descriptor).
    // There is the option of using a descriptor table for constants to reduce their size, but adding a level of
    // indirection, this is probably not needed thanks to bindless existing nowadays!
    // TODO: final -2 is just to add a UAV descriptor slot!
    return std::max<u32>(0, (featureChecker.GetMaxRootSignatureDWORDSize() - 2 * globalConstants.size() - 2)) *
           sizeof(DWORD);
}

ComPtr<ID3D12RootSignature>& DX12ResourceLayout::GetRootSignature()
{
    if (isDirty)
    {
        CompileRootSignature();
        isDirty = false;
    }

    return rootSignature;
}

void DX12ResourceLayout::CompileRootSignature()
{
    u32 rootSignatureDWORDCount = GetMaxLocalConstantSize() / sizeof(DWORD);

    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
    rootParameters.reserve(1 + globalConstants.size());

    // UAV temp for triangle drawing
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = 1280;
        resourceDesc.Height = 600;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        static const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        chk << device->CreateCommittedResource(&heapProps,
                                               D3D12_HEAP_FLAG_NONE,
                                               &resourceDesc,
                                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                               nullptr,
                                               IID_PPV_ARGS(&uavTexture));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0,
        };
        chk << device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ComPtr<ID3D12DescriptorHeap> heapCPU;
        chk << device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapCPU));

        device->CreateUnorderedAccessView(uavTexture.Get(),
                                          nullptr,
                                          nullptr,
                                          heapCPU->GetCPUDescriptorHandleForHeapStart());

        device->CopyDescriptorsSimple(1,
                                      descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                                      heapCPU->GetCPUDescriptorHandleForHeapStart(),
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        CD3DX12_DESCRIPTOR_RANGE uavRange;
        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, // Range type is UAV
                      1,                               // Number of descriptors (just 1)
                      0,                               // Base shader register (u0)
                      0);                              // Register space (space0)
        CD3DX12_ROOT_PARAMETER uavParameter;
        uavParameter.InitAsDescriptorTable(1, &uavRange);
        rootParameters.push_back(std::move(uavParameter));
    }

    CD3DX12_ROOT_PARAMETER rootConstants;
    // Root constants are always bound at the beginning of the root parameters (in slot & space 0).
    rootConstants.InitAsConstants(rootSignatureDWORDCount, 2, 0);
    rootParameters.push_back(std::move(rootConstants));

    // TODO: consider descriptor tables?

    for (const GlobalConstant& constant : globalConstants | std::views::values)
    {
        CD3DX12_ROOT_PARAMETER globalConstantParameter;
        globalConstantParameter.InitAsConstantBufferView(rootSignatureDWORDCount);
        rootParameters.push_back(std::move(globalConstantParameter));

        rootSignatureDWORDCount += 2;
    }

    // TODO: add static samplers!

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(
        static_cast<u32>(rootParameters.size()),
        rootParameters.data(),
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT /*|
                                                                     // Uncomment to activate bindless descriptor heaps!
            (ResourceDescriptorHeap) D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED*/
    );

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    chkSoft << D3D12SerializeRootSignature(&rootSignatureDesc,
                                           D3D_ROOT_SIGNATURE_VERSION_1,
                                           signature.GetAddressOf(),
                                           error.GetAddressOf());
    if (error)
    {
        const char* errorMessage = static_cast<const char*>(error->GetBufferPointer());
        VEX_LOG(Fatal, "Error serializing root signature: {}", errorMessage);
    }

    chk << device->CreateRootSignature(0,
                                       signature->GetBufferPointer(),
                                       signature->GetBufferSize(),
                                       IID_PPV_ARGS(&rootSignature));

    version++;
}

} // namespace vex::dx12