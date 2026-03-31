#pragma once

#include <ShaderCompiler/Compiler/CompilerBase.h>

#if VEX_DX12
#include <DX12/DX12Headers.h>
struct IDxcResult;
#endif

#if VEX_VULKAN
#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#endif

namespace vex::sc
{
#if VEX_VULKAN
ShaderReflection GetSpirvReflection(Span<const byte> spvCode);
#endif

#if VEX_DX12
ShaderReflection GetDxcReflection(const dx12::ComPtr<IDxcResult>& compilationResult);
#endif

} // namespace vex::sc
