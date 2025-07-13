#pragma once

#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/Types.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ResourceLayout;

// Global constant buffer of memory.
// Should be updated infrequently (eg: once per frame).
struct GlobalConstant
{
    std::string name;
    u32 size;
    u32 slot;
    u32 space;
};

// Must be manually unregistered when no longer needed.
struct GlobalConstantHandle
{
    std::string name;
};

// Will automatically unregister the global constant upon destruction.
struct ScopedGlobalConstantHandle
{
    ScopedGlobalConstantHandle(DX12ResourceLayout& globalLayout, std::string name);

    ScopedGlobalConstantHandle(const ScopedGlobalConstantHandle&) = delete;
    ScopedGlobalConstantHandle& operator=(const ScopedGlobalConstantHandle&) = delete;

    ScopedGlobalConstantHandle(ScopedGlobalConstantHandle&&) noexcept;
    ScopedGlobalConstantHandle& operator=(ScopedGlobalConstantHandle&&) noexcept;

    ~ScopedGlobalConstantHandle();

    std::string name{};

private:
    DX12ResourceLayout* globalLayout{};
};

class DX12ResourceLayout : public RHIResourceLayout
{
public:
    DX12ResourceLayout(ComPtr<DX12Device>& device, const DX12FeatureChecker& featureChecker);
    virtual ~DX12ResourceLayout() override;

    ScopedGlobalConstantHandle RegisterScopedGlobalConstant(GlobalConstant globalConstant);
    GlobalConstantHandle RegisterGlobalConstant(GlobalConstant globalConstant);
    void UnregisterGlobalConstant(GlobalConstantHandle globalConstantHandle);
    virtual bool ValidateGlobalConstant(const GlobalConstant& globalConstant) const;

    virtual u32 GetMaxLocalConstantSize() const override;
    virtual u32 GetLocalConstantsOffset() const noexcept override;
    virtual void Update(const ResourceBindingSet& set) override;

    ComPtr<ID3D12RootSignature>& GetRootSignature();

private:
    void CompileRootSignature();

    ComPtr<DX12Device> device;
    const DX12FeatureChecker& featureChecker;
    ComPtr<ID3D12RootSignature> rootSignature;
    std::unordered_map<std::string, GlobalConstant> globalConstants;

    u32 reservedLocalConstantSize;
};

} // namespace vex::dx12