#pragma once

#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <dxgi1_6.h>
#include <dxgicommon.h>
#include <dxgidebug.h>
#include <dxgiformat.h>
#include <wrl/client.h>

namespace vex::dx12
{

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

using DX12Device = ID3D12Device14;

} // namespace vex::dx12