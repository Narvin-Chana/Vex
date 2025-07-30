#include "DX12PipelineState.h"

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Shader.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12GraphicsPipeline.h>
#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12ResourceLayout.h>

namespace vex::dx12
{

DX12GraphicsPipelineState::DX12GraphicsPipelineState(const ComPtr<DX12Device>& device, const Key& key)
    : RHIGraphicsPipelineStateInterface(key)
    , device(device)
{
}

DX12GraphicsPipelineState::~DX12GraphicsPipelineState() = default;

void DX12GraphicsPipelineState::Compile(const Shader& vertexShader,
                                        const Shader& pixelShader,
                                        RHIResourceLayout& resourceLayout)
{
    using namespace GraphicsPipeline;

    auto vsBlob = vertexShader.GetBlob();
    auto psBlob = pixelShader.GetBlob();
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc =
        GetDX12InputElementDescFromVertexInputAssembly(key.vertexInputLayout);
    D3D12_INPUT_LAYOUT_DESC layoutDesc{ .pInputElementDescs = inputElementDesc.data(),
                                        .NumElements = static_cast<u32>(inputElementDesc.size()) };
    std::array<DXGI_FORMAT, 8> rtvFormats = GetRTVFormatsFromRenderTargetState(key.renderTargetState);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
        .pRootSignature = reinterpret_cast<DX12ResourceLayout&>(resourceLayout).GetRootSignature().Get(),
        .VS = CD3DX12_SHADER_BYTECODE(vsBlob.data(), vsBlob.size()),
        .PS = CD3DX12_SHADER_BYTECODE(psBlob.data(), psBlob.size()),
        .BlendState = GetDX12BlendStateFromColorBlendState(key.colorBlendState),
        .SampleMask = UINT_MAX, // Vex does not support MSAA.
        .RasterizerState = GetDX12RasterizerStateFromRasterizerState(key.rasterizerState),
        .DepthStencilState = GetDX12DepthStencilStateFromDepthStencilState(key.depthStencilState),
        .InputLayout = layoutDesc,
        .PrimitiveTopologyType = GetDX12PrimitiveTopologyTypeFromInputAssembly(key.inputAssembly),
        .NumRenderTargets = GetNumRenderTargetsFromRenderTargetState(key.renderTargetState),
        .SampleDesc = { .Count = 1 },
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    std::uninitialized_copy_n(rtvFormats.data(), 8, desc.RTVFormats);
    desc.DSVFormat = TextureFormatToDXGI(key.renderTargetState.depthStencilFormat);

    chk << device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPSO));

#if !VEX_SHIPPING
    chk << graphicsPSO->SetName(StringToWString(std::format("{}", key)).c_str());
#endif
}

void DX12GraphicsPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    // Simple swap and move
    auto cleanupPSO = MakeUnique<DX12GraphicsPipelineState>(device, key);
    std::swap(cleanupPSO->graphicsPSO, graphicsPSO);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

void DX12GraphicsPipelineState::ClearUnsupportedKeyFields(Key& key)
{
    // Unsupported fields are forced to default in order to keep the key's hash consistent.
    key.inputAssembly.primitiveRestartEnabled = true;
    key.rasterizerState.depthClampEnabled = true;
    key.rasterizerState.polygonMode = PolygonMode::Line;
    key.rasterizerState.polygonMode = PolygonMode::Point;
    key.rasterizerState.lineWidth = 0;
    key.depthStencilState.front.reference = 0;
    key.depthStencilState.back.reference = 0;
    key.depthStencilState.minDepthBounds = 0;
    key.depthStencilState.maxDepthBounds = 0;
    key.colorBlendState.logicOpEnabled = true;
    key.colorBlendState.logicOp = LogicOp::Clear;
}

DX12ComputePipelineState::DX12ComputePipelineState(const ComPtr<DX12Device>& device, const Key& key)
    : RHIComputePipelineStateInterface(key)
    , device(device)
{
}

DX12ComputePipelineState::~DX12ComputePipelineState() = default;

void DX12ComputePipelineState::Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout)
{
    auto blob = computeShader.GetBlob();
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
        .pRootSignature = reinterpret_cast<DX12ResourceLayout&>(resourceLayout).GetRootSignature().Get(),
        .CS = CD3DX12_SHADER_BYTECODE(blob.data(), blob.size()),
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    chk << device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&computePSO));
}

void DX12ComputePipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    // Simple swap and move
    auto cleanupPSO = MakeUnique<DX12ComputePipelineState>(device, key);
    std::swap(cleanupPSO->computePSO, computePSO);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

} // namespace vex::dx12