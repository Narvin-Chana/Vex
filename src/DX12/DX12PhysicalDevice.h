#pragma once

#include <Vex/PhysicalDevice.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

struct DX12PhysicalDevice : public PhysicalDevice
{
    ComPtr<IDXGIAdapter4> adapter;
};

} // namespace vex::dx12