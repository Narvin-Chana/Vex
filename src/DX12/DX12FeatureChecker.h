#pragma once

#include <Vex/FeatureChecker.h>

namespace vex::dx12
{
class DX12FeatureChecker : public vex::FeatureChecker
{
public:
    virtual ~DX12FeatureChecker();
    virtual bool IsFeatureSupported(Feature feature) override;
    virtual FeatureLevel GetFeatureLevel() override;
    virtual ResourceBindingTier GetResourceBindingTier() override;
};
} // namespace vex::dx12