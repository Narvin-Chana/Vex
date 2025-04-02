#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace vex::dx12
{

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

}