#include "DX12FeatureChecker.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

#include <DX12/DX12Headers.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

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
        // Can't use magic_enum because D3D_FEATURE_LEVEL_x_x are VEEEERY high numbers and surpasses magic_enum's
        // limits...
        VEX_LOG(Fatal, "Unsupported DX12 feature level.");
    }
    std::unreachable();
}

static ResourceBindingTier ConvertDX12ResourceBindingTierToResourceBindingTier(
    D3D12_RESOURCE_BINDING_TIER resourceBindingTier)
{
    switch (resourceBindingTier)
    {
    case D3D12_RESOURCE_BINDING_TIER_3:
        return ResourceBindingTier::ResourceTier3;
    default:
        VEX_LOG(Fatal, "Unsupported DX12 resource binding tier: {}.", magic_enum::enum_name(resourceBindingTier));
    }
    std::unreachable();
}

static ShaderModel ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel)
{
    switch (shaderModel)
    {
    case D3D_SHADER_MODEL_6_6:
        return ShaderModel::SM_6_6;
    case D3D_SHADER_MODEL_6_7:
        return ShaderModel::SM_6_7;
    // --------------------------------
    // See TODO in vex::ShaderModel!
    // --------------------------------
    // case D3D_SHADER_MODEL_6_8:
    //     return ShaderModel::SM_6_8;
    // case D3D_SHADER_MODEL_6_9:
    //     return ShaderModel::SM_6_9;
    default:
        VEX_LOG(Fatal, "Unsupported shader model: {}.", magic_enum::enum_name(shaderModel));
    }
}

DX12FeatureChecker::DX12FeatureChecker(const ComPtr<ID3D12Device>& device)
{
    adapterName = DXGIFactory::GetDeviceAdapterName(device);

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

std::string_view DX12FeatureChecker::GetPhysicalDeviceName()
{
    return adapterName;
}

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

ShaderModel DX12FeatureChecker::GetShaderModel()
{
    return ConvertDX12ShaderModelToShaderModel(shaderModel.HighestShaderModel);
}

} // namespace vex::dx12