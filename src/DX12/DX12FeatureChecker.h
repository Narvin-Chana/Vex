#pragma once

#include <string>

#include <Vex/FeatureChecker.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{
class DX12FeatureChecker : public vex::FeatureChecker
{
public:
    DX12FeatureChecker(const ComPtr<ID3D12Device>& device);
    virtual ~DX12FeatureChecker();
    virtual bool IsFeatureSupported(Feature feature) const override;
    virtual FeatureLevel GetFeatureLevel() const override;
    virtual ResourceBindingTier GetResourceBindingTier() const override;
    virtual ShaderModel GetShaderModel() const override;

    u32 GetMaxRootSignatureDWORDSize() const;

    static FeatureLevel ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel);
    static D3D_FEATURE_LEVEL ConvertFeatureLevelToDX12FeatureLevel(FeatureLevel featureLevel);
    static ResourceBindingTier ConvertDX12ResourceBindingTierToResourceBindingTier(
        D3D12_RESOURCE_BINDING_TIER resourceBindingTier);
    static ShaderModel ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel);

private:
    // Cached feature support data (to avoid requerying the device).
    D3D12_FEATURE_DATA_D3D12_OPTIONS options;
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1;
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7;
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel;
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels;
    bool rayTracingSupported = false;

    static constexpr D3D12_RAYTRACING_TIER MinimumRayTracingTier = D3D12_RAYTRACING_TIER_1_0;
    static constexpr D3D12_MESH_SHADER_TIER MinimumMeshShaderTier = D3D12_MESH_SHADER_TIER_1;
};
} // namespace vex::dx12