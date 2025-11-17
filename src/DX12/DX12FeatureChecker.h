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
    virtual u32 GetMaxLocalConstantsByteSize() const override;
    virtual bool FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const override;
    bool SupportsTightAlignment() const;

    static FeatureLevel ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel);
    static D3D_FEATURE_LEVEL ConvertFeatureLevelToDX12FeatureLevel(FeatureLevel featureLevel);
    static ResourceBindingTier ConvertDX12ResourceBindingTierToResourceBindingTier(
        D3D12_RESOURCE_BINDING_TIER resourceBindingTier);
    static ShaderModel ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel);

private:
    ComPtr<ID3D12Device> device;

    // Cached feature support data (to avoid requerying the device).
    CD3DX12FeatureSupport featureSupport;
};
} // namespace vex::dx12