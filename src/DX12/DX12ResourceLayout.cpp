#include "DX12ResourceLayout.h"

#include <numeric>
#include <ranges>
#include <utility>

#include <Vex/Logger.h>
#include <Vex/ResourceBindingSet.h>

#include <Vex/Platform/Windows/HResult.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12ResourceLayout::DX12ResourceLayout(ComPtr<DX12Device>& device, const DX12FeatureChecker& featureChecker)
    : device(device)
    , featureChecker(featureChecker)
{
}

DX12ResourceLayout::~DX12ResourceLayout() = default;

u32 DX12ResourceLayout::GetMaxLocalConstantSize() const
{
    return featureChecker.GetMaxRootSignatureDWORDSize() * static_cast<u32>(sizeof(DWORD));
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

    CD3DX12_ROOT_PARAMETER rootConstants;
    // Root constants are always bound at the beginning of the root parameters (in slot & space 0).
    rootConstants.InitAsConstants(rootSignatureDWORDCount, 0, 0);
    rootParameters.push_back(std::move(rootConstants));

    // TODO: add static samplers!

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc =
        CD3DX12_ROOT_SIGNATURE_DESC(static_cast<u32>(rootParameters.size()),
                                    rootParameters.data(),
                                    0,
                                    nullptr,
                                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED 
            // Evaluate the usefulness of bindless samplers, static samplers seem to be easier to map to how Vulkan works.
            /* |
                                        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED*/);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc,
                                             D3D_ROOT_SIGNATURE_VERSION_1,
                                             signature.GetAddressOf(),
                                             error.GetAddressOf());
    if (error)
    {
        const char* errorMessage = static_cast<const char*>(error->GetBufferPointer());
        VEX_LOG(Fatal, "Error serializing root signature: {}", errorMessage);
    }
    else if (FAILED(hr))
    {
        VEX_LOG(Fatal, "Unspecified error serializing root signature: {}", HRToError(hr));
    }

    chk << device->CreateRootSignature(0,
                                       signature->GetBufferPointer(),
                                       signature->GetBufferSize(),
                                       IID_PPV_ARGS(&rootSignature));

    version++;
}

u32 DX12ResourceLayout::GetLocalConstantsOffset() const noexcept
{
    return 0;
}

} // namespace vex::dx12