#include "DX12PipelineState.h"

#include <Vex/RHI/RHIShader.h>

#include <DX12/DX12ResourceLayout.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12GraphicsPipelineState::DX12GraphicsPipelineState(const Key& key)
    : RHIGraphicsPipelineState(key)
{
}

DX12GraphicsPipelineState::~DX12GraphicsPipelineState() = default;

void DX12GraphicsPipelineState::Compile(const RHIShader& vertexShader,
                                        const RHIShader& pixelShader,
                                        RHIResourceLayout& resourceLayout)
{
    // TODO: add missing fields to PSO key!
    auto vsBlob = vertexShader.GetBlob();
    auto psBlob = pixelShader.GetBlob();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
        .pRootSignature = reinterpret_cast<DX12ResourceLayout&>(resourceLayout).GetRootSignature().Get(),
        .VS = CD3DX12_SHADER_BYTECODE(vsBlob.data(), vsBlob.size()),
        .PS = CD3DX12_SHADER_BYTECODE(psBlob.data(), psBlob.size()),
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = { .DepthEnable = false, .StencilEnable = false },
        .InputLayout = { nullptr, 0 },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc = { .Count = 1 },
    };
    // Temp for fullscreen triangle.
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
}

DX12ComputePipelineState::DX12ComputePipelineState(const ComPtr<DX12Device>& device, const Key& key)
    : RHIComputePipelineState(key)
    , device(device)
{
}

DX12ComputePipelineState::~DX12ComputePipelineState() = default;

void DX12ComputePipelineState::Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout)
{
    // TODO: if the PSO is being recompiled while in flight (very likely), this will crash!
    // Should add a way to make outdated resources persist for a complete GPU buffer cycle to avoid this!

    auto blob = computeShader.GetBlob();
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
        .pRootSignature = reinterpret_cast<DX12ResourceLayout&>(resourceLayout).GetRootSignature().Get(),
        .CS = CD3DX12_SHADER_BYTECODE(blob.data(), blob.size()),
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    chk << device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&computePSO));
}

} // namespace vex::dx12