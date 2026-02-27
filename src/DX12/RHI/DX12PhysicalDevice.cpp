#include "DX12PhysicalDevice.h"

#include <Vex/Logger.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/WString.h>

#include <DX12/DX12Formats.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

static constexpr D3D_SHADER_MODEL GMinimumShaderModel = D3D_SHADER_MODEL_6_6;
static constexpr D3D12_RAYTRACING_TIER GMinimumRayTracingTier = D3D12_RAYTRACING_TIER_1_0;
static constexpr D3D12_MESH_SHADER_TIER GMinimumMeshShaderTier = D3D12_MESH_SHADER_TIER_1;
static constexpr D3D_FEATURE_LEVEL GMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;

DX12PhysicalDevice::DX12PhysicalDevice(ComPtr<IDXGIAdapter4> adapter, const ComPtr<ID3D12Device>& device)
    : adapter(std::move(adapter))
    , device{ device }
{
    DXGI_ADAPTER_DESC3 desc;
    this->adapter->GetDesc3(&desc);

    info.deviceName = WStringToString(desc.Description);
    info.dedicatedVideoMemoryMB = static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0);
    chk << featureSupport.Init(device.Get());
}

bool DX12PhysicalDevice::IsFeatureSupported(Feature feature) const
{
    switch (feature)
    {
    case Feature::MeshShader:
        // Check mesh shader support through options7
        return featureSupport.MeshShaderTier() >= GMinimumMeshShaderTier;
    case Feature::RayTracing:
    {
        bool rayTracingSupported = featureSupport.RaytracingTier() >= GMinimumRayTracingTier;
        // For correctness, RT also requires SM_6_3+, although Vex itself requires 6_6.
        rayTracingSupported &= featureSupport.HighestShaderModel() >= D3D_SHADER_MODEL_6_3;
        return rayTracingSupported;
    }
    case Feature::MipGeneration:
        // DX12 has no built-in way to generate mip-maps.
        return false;
    default:
        VEX_LOG(Fatal, "Unable to determine feature support for {}", feature);
        return false;
    }
}

FeatureLevel DX12PhysicalDevice::GetFeatureLevel() const
{
    return ConvertDX12FeatureLevelToFeatureLevel(featureSupport.MaxSupportedFeatureLevel());
}

ResourceBindingTier DX12PhysicalDevice::GetResourceBindingTier() const
{
    return ConvertDX12ResourceBindingTierToResourceBindingTier(featureSupport.ResourceBindingTier());
}

ShaderModel DX12PhysicalDevice::GetShaderModel() const
{
    return ConvertDX12ShaderModelToShaderModel(featureSupport.HighestShaderModel());
}

u32 DX12PhysicalDevice::GetMaxLocalConstantsByteSize() const
{
    // 64 DWORDS is the hard-coded DX12 limit for root signatures.
    return 64 * sizeof(DWORD);
}

bool DX12PhysicalDevice::FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{ .Format = TextureFormatToDXGI(format, isSRGB) };
    chk << device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));

    const bool supportsLinearFiltering = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
    return supportsLinearFiltering;
}

bool DX12PhysicalDevice::SupportsTightAlignment() const
{
    return featureSupport.TightAlignmentSupportTier() > D3D12_TIGHT_ALIGNMENT_TIER_NOT_SUPPORTED;
}

bool DX12PhysicalDevice::SupportsMinimalRequirements() const
{
    // Vex requires a minimum feature level of 12_1.
    if (featureSupport.MaxSupportedFeatureLevel() < GMinimumFeatureLevel)
    {
        return false;
    }

    // Vex requires DX12's EnhancedBarriers for GPU resource synchronization.
    if (!featureSupport.EnhancedBarriersSupported())
    {
        return false;
    }

    // Vex requires SM6_6 for bindless (currently a hard requirement due to Vex not supporting "bindful" code).
    if (featureSupport.HighestShaderModel() < GMinimumShaderModel)
    {
        return false;
    }

    return true;
}

FeatureLevel DX12PhysicalDevice::ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel)
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

D3D_FEATURE_LEVEL DX12PhysicalDevice::ConvertFeatureLevelToDX12FeatureLevel(FeatureLevel featureLevel)
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

ResourceBindingTier DX12PhysicalDevice::ConvertDX12ResourceBindingTierToResourceBindingTier(
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

ShaderModel DX12PhysicalDevice::ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel)
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
