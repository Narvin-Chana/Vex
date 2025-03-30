#pragma once

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

void InitializeDebugLayer(bool enableGPUDebugLayer, bool enableGPUBasedValidation);
void SetupDebugMessageCallback(const ComPtr<DX12Device>& device);
void CleanupDebugMessageCallback(const ComPtr<DX12Device>& device);
} // namespace vex::dx12