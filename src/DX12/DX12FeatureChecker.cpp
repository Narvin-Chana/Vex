#include "DX12FeatureChecker.h"

#include <DX12/HRChecker.h>
#include <Vex/Logger.h>

namespace vex::dx12
{

static FeatureLevel ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
    case D3D_FEATURE_LEVEL_12_1:
        return FeatureLevel::Level_12_1;
    case D3D_FEATURE_LEVEL_12_2:
        return FeatureLevel::Level_12_2;
    default:
        VEX_LOG(Fatal, "Unsupported DX12 feature level.");
    }
    std::unreachable();
}

static ResourceBindingTier ConvertDX12ResourceBindingTierToResourceBindingTier(
    D3D12_RESOURCE_BINDING_TIER resourceBindingTier)
{
    switch (resourceBindingTier)
    {
    case D3D12_RESOURCE_BINDING_TIER_1:
        return ResourceBindingTier::ResourceTier1;
    case D3D12_RESOURCE_BINDING_TIER_2:
        return ResourceBindingTier::ResourceTier2;
    case D3D12_RESOURCE_BINDING_TIER_3:
        return ResourceBindingTier::ResourceTier3;
    default:
        VEX_LOG(Fatal, "Unsupported DX12 resource binding tier.");
    }
    std::unreachable();
}

DX12FeatureChecker::DX12FeatureChecker(const ComPtr<ID3D12Device>& device)
{
    // Try to get device5 interface (needed for ray tracing)
    rayTracingSupported = false;
    ComPtr<ID3D12Device5> device5;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&device5))))
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
        {
            rayTracingSupported = options5.RaytracingTier >= MinimumRayTracingTier;
        }
    }

    // Initialize feature level check data
    static const D3D_FEATURE_LEVEL d3dFeatureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1 };
    featureLevels.NumFeatureLevels = _countof(d3dFeatureLevels);
    featureLevels.pFeatureLevelsRequested = d3dFeatureLevels;

    // Initialize shader model data
    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_7;

    chk << device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
    chk << device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1));
    chk << device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
    chk << device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
    chk << device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
}

DX12FeatureChecker::~DX12FeatureChecker() = default;

bool DX12FeatureChecker::IsFeatureSupported(Feature feature)
{
    switch (feature)
    {
    case Feature::MeshShader:
        // Check mesh shader support through options7
        return options7.MeshShaderTier >= MinimumMeshShaderTier;
    case Feature::RayTracing:
        return rayTracingSupported;
    default:
        return false;
    }
}

FeatureLevel DX12FeatureChecker::GetFeatureLevel()
{
    return ConvertDX12FeatureLevelToFeatureLevel(featureLevels.MaxSupportedFeatureLevel);
}

ResourceBindingTier DX12FeatureChecker::GetResourceBindingTier()
{
    return ConvertDX12ResourceBindingTierToResourceBindingTier(options.ResourceBindingTier);
}

} // namespace vex::dx12