#pragma once

#include <Vex/Formats.h>
#include <Vex/Types.h>

#include <RHI/RHIPhysicalDevice.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12PhysicalDevice final : public RHIPhysicalDeviceBase
{
public:
    DX12PhysicalDevice(ComPtr<IDXGIAdapter4> adapter, const ComPtr<ID3D12Device>& device);
    ~DX12PhysicalDevice() = default;

    DX12PhysicalDevice(const DX12PhysicalDevice&) = delete;
    DX12PhysicalDevice& operator=(const DX12PhysicalDevice&) = delete;

    DX12PhysicalDevice(DX12PhysicalDevice&&) = default;
    DX12PhysicalDevice& operator=(DX12PhysicalDevice&&) = default;

    virtual bool IsFeatureSupported(Feature feature) const override;
    virtual FeatureLevel GetFeatureLevel() const override;
    virtual ResourceBindingTier GetResourceBindingTier() const override;
    virtual ShaderModel GetShaderModel() const override;
    virtual u32 GetMaxLocalConstantsByteSize() const override;
    virtual bool FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const override;

    bool SupportsTightAlignment() const;
    bool SupportsMinimalRequirements() const override;

    static FeatureLevel ConvertDX12FeatureLevelToFeatureLevel(D3D_FEATURE_LEVEL featureLevel);
    static D3D_FEATURE_LEVEL ConvertFeatureLevelToDX12FeatureLevel(FeatureLevel featureLevel);
    static ResourceBindingTier ConvertDX12ResourceBindingTierToResourceBindingTier(
        D3D12_RESOURCE_BINDING_TIER resourceBindingTier);
    static ShaderModel ConvertDX12ShaderModelToShaderModel(D3D_SHADER_MODEL shaderModel);

    ComPtr<IDXGIAdapter4> adapter;
    ComPtr<ID3D12Device> device;

    // Cached feature support data (to avoid requerying the device).
    CD3DX12FeatureSupport featureSupport;
};

} // namespace vex::dx12