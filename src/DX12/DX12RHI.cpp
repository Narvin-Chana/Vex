#include "DX12RHI.h"

namespace vex::dx12
{

DX12RHI::~DX12RHI() = default;

FeatureChecker& DX12RHI::GetFeatureChecker()
{
    return featureChecker;
}

} // namespace vex::dx12