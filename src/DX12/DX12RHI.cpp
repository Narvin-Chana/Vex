#include "DX12RHI.h"

#include <DX12/DXGIFactory.h>
#include <Vex/Logger.h>

namespace vex::dx12
{

DX12RHI::DX12RHI(bool enableGPUDebugLayer, bool enableGPUBasedValidation)
    : featureChecker(DXGIFactory::CreateDevice(nullptr))
{
    HMODULE d3d12Module = GetModuleHandleA("D3D12Core.dll");
    if (d3d12Module)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(d3d12Module, path, MAX_PATH);
        VEX_LOG(Info,
                "Using D3D12-SDK: {0}\n\tIf this path is in the project's target directory (and not in SYSTEM32), you "
                "are correctly using the D3D12-Agility-SDK!",
                std::string(path));
    }
}

DX12RHI::~DX12RHI() = default;

FeatureChecker& DX12RHI::GetFeatureChecker()
{
    return featureChecker;
}

} // namespace vex::dx12