#include "DX12FeatureChecker.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/Utility/Formattable.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12Headers.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

static constexpr D3D_SHADER_MODEL GMinimumShaderModel = D3D_SHADER_MODEL_6_6;
static constexpr D3D12_RAYTRACING_TIER GMinimumRayTracingTier = D3D12_RAYTRACING_TIER_1_0;
static constexpr D3D12_MESH_SHADER_TIER GMinimumMeshShaderTier = D3D12_MESH_SHADER_TIER_1;
static constexpr D3D_FEATURE_LEVEL GMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;

DX12FeatureChecker::DX12FeatureChecker(const ComPtr<ID3D12Device>& device)
    : device{ device }
{
    // Try to get device5 interface (needed for ray tracing)
    chk << featureSupport.Init(device.Get());

    // Vex requires a minimum feature level of 12_1.
    if (featureSupport.MaxSupportedFeatureLevel() < GMinimumFeatureLevel)
    {
        VEX_LOG(Fatal,
                "DX12RHI incompatible: Vex DX12RHI requires feature level 12_1 which is not supported by your GPU.");
    }

    // Vex requires DX12's EnhancedBarriers for GPU resource synchronization.
    if (!featureSupport.EnhancedBarriersSupported())
    {
        VEX_LOG(Fatal, "DX12RHI incompatible: Vex DX12RHI uses Enhanced Barriers which are not supported by your GPU.");
    }

    // Vex requires SM6_6 for bindless (currently a hard requirement due to Vex not supporting "bindful" code).
    if (featureSupport.HighestShaderModel() < GMinimumShaderModel)
    {
        VEX_LOG(Fatal,
                "DX12RHI incompatible: Vex's DX12 implementation requires at least SM_6_6 for the untyped "
                "ResourceDescriptorHeap feature.");
    }
}

DX12FeatureChecker::~DX12FeatureChecker() = default;

bool DX12FeatureChecker::IsFeatureSupported(Feature feature) const
{
    switch (feature)
    {
    case Feature::MeshShader:
        // Check mesh shader support through options7
        return featureSupport.MeshShaderTier() >= GMinimumMeshShaderTier;
    case Feature::RayTracing:
    {
        bool rayTracingSupported = featureSupport.RaytracingTier() >= GMinimumRayTracingTier;
        // For correctness, RT also requires SM_6_3+.
        rayTracingSupported &= featureSupport.HighestShaderModel() >= D3D_SHADER_MODEL_6_3;
        return rayTracingSupported;
    }
    case Feature::BindlessResources:
        return featureSupport.HighestShaderModel() >= GMinimumShaderModel;
    case Feature::MipGeneration:
        // DX12 has no built-in way to generate mip-maps.
        return false;
    default:
        VEX_LOG(Fatal, "Unable to determine feature support for {}", feature);
        return false;
    }
}

FeatureLevel DX12FeatureChecker::GetFeatureLevel() const
{
    return ConvertDX12FeatureLevelToFeatureLevel(featureSupport.MaxSupportedFeatureLevel());
}

ResourceBindingTier DX12FeatureChecker::GetResourceBindingTier() const
{
    return ConvertDX12ResourceBindingTierToResourceBindingTier(featureSupport.ResourceBindingTier());
}

ShaderModel DX12FeatureChecker::GetShaderModel() const
{
    return ConvertDX12ShaderModelToShaderModel(featureSupport.HighestShaderModel());
}

u32 DX12FeatureChecker::GetMaxLocalConstantsByteSize() const
{
    // 64 DWORDS is the hard-coded DX12 limit for root signatures.
    return 64 * sizeof(DWORD);
}

bool DX12FeatureChecker::FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{ .Format = TextureFormatToDXGI(format, isSRGB) };
    chk << device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));

    const bool supportsLinearFiltering = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
    return supportsLinearFiltering;
}

bool DX12FeatureChecker::SupportsTightAlignment() const
{
    return featureSupport.TightAlignmentSupportTier() > D3D12_TIGHT_ALIGNMENT_TIER_NOT_SUPPORTED;
}

FeatureLevel DX12FeatureChecker::ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
    case D3D_FEATURE_LEVEL_12_0:
        return FeatureLevel::Level_12_0;
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

D3D_FEATURE_LEVEL DX12FeatureChecker::ConvertFeatureLevelToDX12FeatureLevel(FeatureLevel featureLevel)
{
    switch (featureLevel)
    {
    case FeatureLevel::Level_12_0:
        return D3D_FEATURE_LEVEL_12_0;
    case FeatureLevel::Level_12_1:
        return D3D_FEATURE_LEVEL_12_1;
    case FeatureLevel::Level_12_2:
        return D3D_FEATURE_LEVEL_12_2;
    default:
        VEX_LOG(Fatal, "Unsupported feature level: {}.", featureLevel);
    }
    std::unreachable();
}

ResourceBindingTier DX12FeatureChecker::ConvertDX12ResourceBindingTierToResourceBindingTier(
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

ShaderModel DX12FeatureChecker::ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel)
{
    using enum ShaderModel;
    switch (shaderModel)
    {
    case D3D_SHADER_MODEL_6_0:
        return SM_6_0;
    case D3D_SHADER_MODEL_6_1:
        return SM_6_1;
    case D3D_SHADER_MODEL_6_2:
        return SM_6_2;
    case D3D_SHADER_MODEL_6_3:
        return SM_6_3;
    case D3D_SHADER_MODEL_6_4:
        return SM_6_4;
    case D3D_SHADER_MODEL_6_5:
        return SM_6_5;
    case D3D_SHADER_MODEL_6_6:
        return SM_6_6;
    case D3D_SHADER_MODEL_6_7:
        return SM_6_7;
    case D3D_SHADER_MODEL_6_8:
        return SM_6_8;
    case D3D_SHADER_MODEL_6_9:
        return SM_6_9;
    default:
        VEX_LOG(Fatal, "Unsupported shader model: {}.", magic_enum::enum_name(shaderModel));
    }
    std::unreachable();
}

} // namespace vex::dx12