#include "DX12Headers.h"

// This file is included in the client application (via CMake's vex_setup_d3d12_agility_runtime function).
// This must be the case as DX12 is loaded dynamically at startup which only parses definitions from the main
// application. An example can be found in "${VEX_ROOT}/examples/CMakeLists.txt"

#ifdef D3D12_AGILITY_SDK_ENABLED

extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = DIRECTX_AGILITY_SDK_VERSION;
}

extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

#endif