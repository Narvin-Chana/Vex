#include "DX12RHI.h"

#include <DX12/DXGIFactory.h>

namespace vex::dx12
{

DX12RHI::DX12RHI()
    : featureChecker(DXGIFactory::CreateDevice(nullptr))
{
}

DX12RHI::~DX12RHI() = default;

FeatureChecker& DX12RHI::GetFeatureChecker()
{
    return featureChecker;
}

} // namespace vex::dx12