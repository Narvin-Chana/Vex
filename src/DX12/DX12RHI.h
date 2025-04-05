#pragma once

#include <Vex/RHI.h>

#include <DX12/DX12FeatureChecker.h>

namespace vex
{
struct PlatformWindow;
}

namespace vex::dx12
{

class DX12RHI : public vex::RHI
{
public:
    DX12RHI(bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~DX12RHI() override;
    virtual FeatureChecker& GetFeatureChecker() override;

private:
    DX12FeatureChecker featureChecker;
};

} // namespace vex::dx12