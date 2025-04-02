#include "DXGIFactory.h"

#include <DX12/HRChecker.h>

namespace vex::dx12
{

ComPtr<ID3D12Device> DXGIFactory::CreateDevice(IDXGIAdapter* adapter)
{
    ComPtr<ID3D12Device10> device;
    // TODO: convert feature level from FeatureChecker::MinimumFeatureLevel to D3D12_FEATURE_LEVEL
    chk << D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));
    return device;
}

} // namespace vex::dx12
