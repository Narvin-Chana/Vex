#pragma once

#include <DX12/DX12Types.h>

namespace vex::dx12
{
class DXGIFactory
{
public:
    static ComPtr<ID3D12Device> CreateDevice(IDXGIAdapter* adapter);
};

} // namespace vex::dx12