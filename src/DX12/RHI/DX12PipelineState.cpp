#include "DX12PipelineState.h"

#include <Vex/ByteUtils.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Shaders/Shader.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12GraphicsPipeline.h>
#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12ResourceLayout.h>

namespace vex::dx12
{

DX12GraphicsPipelineState::DX12GraphicsPipelineState(const ComPtr<DX12Device>& device, const Key& key)
    : RHIGraphicsPipelineStateBase(key)
    , device(device)
{
}

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
        .pRootSignature = resourceLayout.GetRootSignature().Get(),
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

    // Update versions for staleness purposes.
    rootSignatureVersion = resourceLayout.version;
    vertexShaderVersion = vertexShader.version;
    pixelShaderVersion = pixelShader.version;

#if !VEX_SHIPPING
    chk << graphicsPSO->SetName(StringToWString(std::format("GraphicsPSO: {}", key)).c_str());
#endif
}

void DX12GraphicsPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    if (!graphicsPSO)
    {
        return;
    }
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

void DX12ComputePipelineState::Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout)
{
    auto blob = computeShader.GetBlob();
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
        .pRootSignature = resourceLayout.GetRootSignature().Get(),
        .CS = CD3DX12_SHADER_BYTECODE(blob.data(), blob.size()),
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    chk << device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&computePSO));

    // Update versions for staleness purposes.
    rootSignatureVersion = resourceLayout.version;
    computeShaderVersion = computeShader.version;

#if !VEX_SHIPPING
    chk << computePSO->SetName(StringToWString(std::format("ComputePSO: {}", key)).c_str());
#endif
}

void DX12ComputePipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    if (!computePSO)
    {
        return;
    }
    // Simple swap and move
    auto cleanupPSO = MakeUnique<DX12ComputePipelineState>(device, key);
    std::swap(cleanupPSO->computePSO, computePSO);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

DX12RayTracingPipelineState::DX12RayTracingPipelineState(const ComPtr<DX12Device>& device, const Key& key)
    : RHIRayTracingPipelineStateInterface(key)
    , device(device)
{
}

void DX12RayTracingPipelineState::Compile(const RayTracingShaderCollection& shaderCollection,
                                          RHIResourceLayout& resourceLayout,
                                          ResourceCleanup& resourceCleanup,
                                          RHIAllocator& allocator)
{
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // Ray generation shader
    CD3DX12_DXIL_LIBRARY_SUBOBJECT* rayGenerationLib =
        raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE rayGenBC = { shaderCollection.rayGenerationShader->GetBlob().data(),
                                       shaderCollection.rayGenerationShader->GetBlob().size() };
    rayGenerationLib->SetDXILLibrary(&rayGenBC);
    rayGenerationLib->DefineExport(StringToWString(shaderCollection.rayGenerationShader->key.entryPoint).c_str());

    // Ray miss shaders
    for (NonNullPtr<Shader> missShader : shaderCollection.rayMissShaders)
    {
        CD3DX12_DXIL_LIBRARY_SUBOBJECT* missLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE missBC = { missShader->GetBlob().data(), missShader->GetBlob().size() };
        missLib->SetDXILLibrary(&missBC);
        missLib->DefineExport(StringToWString(missShader->key.entryPoint).c_str());
    }

    // Hit group shaders
    for (const auto& hitGroup : shaderCollection.hitGroupShaders)
    {
        CD3DX12_HIT_GROUP_SUBOBJECT* hitGroupSubObj = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

        // Set the hit group name
        std::wstring hitGroupName = StringToWString(hitGroup.name);
        hitGroupSubObj->SetHitGroupExport(hitGroupName.c_str());

        // Set hit group type (triangles vs procedural)
        D3D12_HIT_GROUP_TYPE hitGroupType = hitGroup.rayIntersectionShader.has_value()
                                                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                                                : D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitGroupSubObj->SetHitGroupType(hitGroupType);

        // Closest hit shader
        {
            CD3DX12_DXIL_LIBRARY_SUBOBJECT* closestHitLib =
                raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            D3D12_SHADER_BYTECODE closestHitBC = { hitGroup.rayClosestHitShader->GetBlob().data(),
                                                   hitGroup.rayClosestHitShader->GetBlob().size() };
            closestHitLib->SetDXILLibrary(&closestHitBC);
            closestHitLib->DefineExport(StringToWString(hitGroup.rayClosestHitShader->key.entryPoint).c_str());
            hitGroupSubObj->SetClosestHitShaderImport(
                StringToWString(hitGroup.rayClosestHitShader->key.entryPoint).c_str());
        }

        // Any hit shader
        if (hitGroup.rayAnyHitShader)
        {
            CD3DX12_DXIL_LIBRARY_SUBOBJECT* anyHitLib =
                raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            D3D12_SHADER_BYTECODE anyHitBC = { hitGroup.rayAnyHitShader.value()->GetBlob().data(),
                                               hitGroup.rayAnyHitShader.value()->GetBlob().size() };
            anyHitLib->SetDXILLibrary(&anyHitBC);
            anyHitLib->DefineExport(StringToWString(hitGroup.rayAnyHitShader.value()->key.entryPoint).c_str());

            hitGroupSubObj->SetAnyHitShaderImport(
                StringToWString(hitGroup.rayAnyHitShader.value()->key.entryPoint).c_str());
        }

        // Ray intersection shader
        if (hitGroup.rayIntersectionShader)
        {
            CD3DX12_DXIL_LIBRARY_SUBOBJECT* intersectionLib =
                raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            D3D12_SHADER_BYTECODE intersectionBC = { hitGroup.rayIntersectionShader.value()->GetBlob().data(),
                                                     hitGroup.rayIntersectionShader.value()->GetBlob().size() };
            intersectionLib->SetDXILLibrary(&intersectionBC);
            intersectionLib->DefineExport(
                StringToWString(hitGroup.rayIntersectionShader.value()->key.entryPoint).c_str());

            hitGroupSubObj->SetIntersectionShaderImport(
                StringToWString(hitGroup.rayIntersectionShader.value()->key.entryPoint).c_str());
        }
    }

    for (NonNullPtr<Shader> callableShader : shaderCollection.rayCallableShaders)
    {
        CD3DX12_DXIL_LIBRARY_SUBOBJECT* callableLib =
            raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE intersectionBC = { callableShader->GetBlob().data(), callableShader->GetBlob().size() };
        callableLib->SetDXILLibrary(&intersectionBC);
        callableLib->DefineExport(StringToWString(callableShader->key.entryPoint).c_str());
    }

    // Shader Config - defines payload and attribute sizes
    CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig =
        raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    u32 payloadSize = key.maxPayloadByteSize;     // e.g., sizeof(RayPayload)
    u32 attributeSize = key.maxAttributeByteSize; // e.g., sizeof(BuiltInTriangleIntersectionAttributes) = 8 bytes
    shaderConfig->Config(payloadSize, attributeSize);

    // Use global root signature in resourceLayout.
    CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignature =
        raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(resourceLayout.GetRootSignature().Get());

    // Pipeline Config - defines max trace recursion depth.
    CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pipelineConfig =
        raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    u32 maxTraceRecursionDepth = key.maxRecursionDepth;
    pipelineConfig->Config(maxTraceRecursionDepth);

    chk << device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&stateObject));

    GenerateIdentifiers(shaderCollection);

    CreateShaderTables(resourceCleanup, allocator);

    UpdateVersions(shaderCollection, resourceLayout);
}

void DX12RayTracingPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    if (!(stateObject && rayGenerationShaderTable && rayMissShaderTable && hitGroupShaderTable &&
          rayCallableShaderTable))
    {
        return;
    }

    // Simple swap and move
    auto cleanupPSO = MakeUnique<DX12RayTracingPipelineState>(device, key);
    // State object
    std::swap(cleanupPSO->stateObject, stateObject);
    // Shader tables
    std::swap(cleanupPSO->rayGenerationShaderTable, rayGenerationShaderTable);
    std::swap(cleanupPSO->rayMissShaderTable, rayMissShaderTable);
    std::swap(cleanupPSO->hitGroupShaderTable, hitGroupShaderTable);
    std::swap(cleanupPSO->rayCallableShaderTable, rayCallableShaderTable);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

void DX12RayTracingPipelineState::PrepareDispatchRays(D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc) const
{
    if (rayGenerationShaderTable)
    {
        dispatchRaysDesc.RayGenerationShaderRecord = rayGenerationShaderTable->GetVirtualAddressRange();
    }

    if (rayMissShaderTable)
    {
        dispatchRaysDesc.MissShaderTable = rayMissShaderTable->GetVirtualAddressRangeAndStride();
    }

    if (hitGroupShaderTable)
    {
        dispatchRaysDesc.HitGroupTable = hitGroupShaderTable->GetVirtualAddressRangeAndStride();
    }

    if (rayCallableShaderTable)
    {
        dispatchRaysDesc.CallableShaderTable = rayCallableShaderTable->GetVirtualAddressRangeAndStride();
    }
}

void DX12RayTracingPipelineState::GenerateIdentifiers(const RayTracingShaderCollection& shaderCollection)
{
    ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    chk << stateObject->QueryInterface(IID_PPV_ARGS(&stateObjectProperties));

    rayGenerationIdentifier = stateObjectProperties->GetShaderIdentifier(
        StringToWString(shaderCollection.rayGenerationShader->key.entryPoint).c_str());

    for (NonNullPtr<Shader> missShader : shaderCollection.rayMissShaders)
    {
        void* identifier =
            stateObjectProperties->GetShaderIdentifier(StringToWString(missShader->key.entryPoint).c_str());
        rayMissIdentifiers.push_back(identifier);
    }

    for (const auto& hitGroupData : shaderCollection.hitGroupShaders)
    {
        void* identifier = stateObjectProperties->GetShaderIdentifier(StringToWString(hitGroupData.name).c_str());
        hitGroupIdentifiers.push_back(identifier);
    }

    for (NonNullPtr<Shader> callableShader : shaderCollection.rayCallableShaders)
    {
        void* identifier =
            stateObjectProperties->GetShaderIdentifier(StringToWString(callableShader->key.entryPoint).c_str());
        rayCallableIdentifiers.push_back(identifier);
    }
}

void DX12RayTracingPipelineState::CreateShaderTables(ResourceCleanup& resourceCleanup, RHIAllocator& allocator)
{
    if (rayGenerationShaderTable.has_value())
    {
        resourceCleanup.CleanupResource(std::move(rayGenerationShaderTable->buffer));
    }
    if (rayMissShaderTable.has_value())
    {
        resourceCleanup.CleanupResource(std::move(rayMissShaderTable->buffer));
    }
    if (hitGroupShaderTable.has_value())
    {
        resourceCleanup.CleanupResource(std::move(hitGroupShaderTable->buffer));
    }
    if (rayCallableShaderTable.has_value())
    {
        resourceCleanup.CleanupResource(std::move(rayCallableShaderTable->buffer));
    }

    BufferDescription shaderTableDescription{
        .usage = BufferUsage::GenericBuffer,
        .memoryLocality = ResourceMemoryLocality::CPUWrite,
    };

    shaderTableDescription.name = "RayGenerationShaderTable";
    shaderTableDescription.byteSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    rayGenerationShaderTable =
        DX12ShaderTable(device, allocator, shaderTableDescription, { &rayGenerationIdentifier, 1 });

    if (!rayMissIdentifiers.empty())
    {
        shaderTableDescription.name = "RayMissShadersTable";
        shaderTableDescription.byteSize =
            AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) *
            rayMissIdentifiers.size();
        rayMissShaderTable = DX12ShaderTable(device, allocator, shaderTableDescription, rayMissIdentifiers);
    }

    if (!hitGroupIdentifiers.empty())
    {
        shaderTableDescription.name = "HitGroupShadersTable";
        shaderTableDescription.byteSize =
            AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) *
            hitGroupIdentifiers.size();
        hitGroupShaderTable = DX12ShaderTable(device, allocator, shaderTableDescription, hitGroupIdentifiers);
    }

    if (!rayCallableIdentifiers.empty())
    {
        shaderTableDescription.name = "RayCallableShadersTable";
        shaderTableDescription.byteSize =
            AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) *
            rayCallableIdentifiers.size();

        rayCallableShaderTable = DX12ShaderTable(device, allocator, shaderTableDescription, rayCallableIdentifiers);
    }
}

void DX12RayTracingPipelineState::UpdateVersions(const RayTracingShaderCollection& shaderCollection,
                                                 RHIResourceLayout& resourceLayout)
{
    rootSignatureVersion = resourceLayout.version;
    rayGenerationShaderVersion = shaderCollection.rayGenerationShader->version;
    rayMissShaderVersions.resize(shaderCollection.rayMissShaders.size());
    for (u32 i = 0; i < shaderCollection.rayMissShaders.size(); ++i)
    {
        rayMissShaderVersions[i] = shaderCollection.rayMissShaders[i]->version;
    }

    hitGroupVersions.resize(shaderCollection.hitGroupShaders.size());
    for (u32 i = 0; i < shaderCollection.hitGroupShaders.size(); ++i)
    {
        hitGroupVersions[i].rayClosestHitVersion = shaderCollection.hitGroupShaders[i].rayClosestHitShader->version;
        if (shaderCollection.hitGroupShaders[i].rayAnyHitShader)
        {
            hitGroupVersions[i].rayAnyHitVersion = shaderCollection.hitGroupShaders[i].rayAnyHitShader.value()->version;
        }
        if (shaderCollection.hitGroupShaders[i].rayIntersectionShader)
        {
            hitGroupVersions[i].rayIntersectionVersion =
                shaderCollection.hitGroupShaders[i].rayIntersectionShader.value()->version;
        }
    }

    rayCallableShaderVersions.resize(shaderCollection.rayCallableShaders.size());
    for (u32 i = 0; i < shaderCollection.rayCallableShaders.size(); ++i)
    {
        rayCallableShaderVersions[i] = shaderCollection.rayCallableShaders[i]->version;
    }
}

} // namespace vex::dx12