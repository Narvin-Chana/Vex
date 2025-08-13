#include "DX12ResourceLayout.h"

#include <numeric>
#include <ranges>
#include <utility>

#include <Vex/ComputeResources.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Windows/HResult.h>

#include <DX12/DX12TextureSampler.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12ResourceLayout::DX12ResourceLayout(ComPtr<DX12Device>& device)
    : device(device)
{
}

DX12ResourceLayout::~DX12ResourceLayout() = default;

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
    u32 rootSignatureDWORDCount = GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize() / sizeof(DWORD);

    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;

    CD3DX12_ROOT_PARAMETER rootCBV{};
    // Root constant buffer is bound at the first slot (for Vex's internal bindless mapping).
    rootCBV.InitAsConstantBufferView(0);
    rootParameters.push_back(std::move(rootCBV));

    CD3DX12_ROOT_PARAMETER rootConstants{};
    // Root constants are always bound at slot 1 of the root parameters (in space 0).
    rootConstants.InitAsConstants(rootSignatureDWORDCount - 2, 1);
    rootParameters.push_back(std::move(rootConstants));

    std::vector<D3D12_STATIC_SAMPLER_DESC> dxSamplers =
        GraphicsPipeline::GetDX12StaticSamplersFromTextureSamplers(samplers);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc =
        CD3DX12_ROOT_SIGNATURE_DESC(static_cast<u32>(rootParameters.size()),
                                    rootParameters.data(),
                                    static_cast<u32>(dxSamplers.size()),
                                    dxSamplers.data(),
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

} // namespace vex::dx12