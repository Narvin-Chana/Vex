#pragma once

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <directx/dxgicommon.h>
#include <directx/dxgiformat.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace vex::dx12
{

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

}