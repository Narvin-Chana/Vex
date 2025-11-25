#pragma once
#include <span>

#include <Vex/Shaders/CompilerBase.h>

#if VEX_DX12
#include <DX12/DX12Headers.h>
struct IDxcResult;
#endif

#if VEX_VULKAN
#include <Vex/Types.h>
#endif

namespace slang
{
struct IComponentType;
}

namespace vex
{
#if VEX_VULKAN
ShaderReflection GetSpirvReflection(std::span<const byte> spvCode);
#endif

#if VEX_DX12
ShaderReflection GetDxcReflection(dx12::ComPtr<IDxcResult>);
#endif

ShaderReflection GetSlangReflection(slang::IComponentType* program);

} // namespace vex
