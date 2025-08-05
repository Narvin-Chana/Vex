#pragma once

#include <string_view>

namespace vex
{

constexpr std::string_view ShaderGenLocalConstantName = "zzzZZZ___VEX_LOCAL_CONSTANT";

constexpr std::string_view ShaderGenGeneralMacros = R"(
// GENERAL MACROS -------------------------

#if defined(VEX_DX12)

#define zzzZZZ___VEX_LOCAL_CONSTANT

#elif defined(VEX_VULKAN)

#define zzzZZZ___VEX_LOCAL_CONSTANT [[vk::push_constant]]

#endif
)";

constexpr std::string_view ShaderGenBindingMacros = R"(

// BINDING MACROS -------------------------

// Usage: VEX_GLOBAL_RESOURCE(StructuredBuffer<Colors>, ColorBuffer);
// Can now use ColorBuffer in your code as any other StructuredBuffer.
#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

// Usage: StructuredBuffer<MyStruct> myStruct = VEX_GET_BINDLESS_RESOURCE(index);
// Can now use myStruct in your code as any other StructuredBuffer.
#define VEX_GET_BINDLESS_RESOURCE(index) ResourceDescriptorHeap[index];

#define VEX_LOCAL_CONSTANTS(type, name) zzzZZZ___VEX_LOCAL_CONSTANT ConstantBuffer<type> name : register(b1);

)";

} // namespace vex
