#include "DX12FeatureChecker.h"

namespace vex::dx12
{

DX12FeatureChecker::~DX12FeatureChecker() = default;

bool DX12FeatureChecker::IsFeatureSupported(Feature feature)
{
    return false;
}

FeatureLevel DX12FeatureChecker::GetFeatureLevel()
{
    return FeatureLevel();
}

ResourceBindingTier DX12FeatureChecker::GetResourceBindingTier()
{
    return ResourceBindingTier();
}

} // namespace vex::dx12