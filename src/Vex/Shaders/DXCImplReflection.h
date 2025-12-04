#pragma once
#include <Vex/Containers/Span.h>

#include <Vex/Shaders/CompilerBase.h>

#if VEX_DX12
#include <DX12/DX12Headers.h>
struct IDxcResult;
#endif

#if VEX_VULKAN
#include <Vex/Types.h>
#endif

namespace vex
{
#if VEX_VULKAN
ShaderReflection GetSpirvReflection(Span<const byte> spvCode);
#endif

#if VEX_DX12
ShaderReflection GetDxcReflection(dx12::ComPtr<IDxcResult>);
#endif

} // namespace vex
