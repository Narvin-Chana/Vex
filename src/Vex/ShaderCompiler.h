// #pragma once
//
// #if defined(_WIN32) and VEX_DX12
// #define WIN32_LEAN_AND_MEAN
// #include <Windows.h>
// #endif
//
// #include <dxcapi.h>
//
// namespace vex
//{
//
// struct CompilerUtil
//{
//     CompilerUtil();
//     ~CompilerUtil();
//
//     // Do without ComPtr to keep this cross-platform
//     IDxcCompiler3* compiler;
//     IDxcUtils* utils;
//     IDxcIncludeHandler* defaultIncludeHandler;
// };
//
// } // namespace vex