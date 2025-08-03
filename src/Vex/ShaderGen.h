#pragma once

constexpr const char* ShaderGenGeneralMacros = R"(
// GENERAL MACROS -------------------------

#if defined(VEX_DX12)

#define VEX_LOCAL_CONSTANT

#elif defined(VEX_VULKAN)

#define VEX_LOCAL_CONSTANT [[vk::push_constant]]

#endif
)";

constexpr const char* ShaderGenBindingMacros = R"(

// BINDING MACROS -------------------------

// Usage: VEX_GLOBAL_RESOURCE(StructuredBuffer<Colors>, ColorBuffer);
// Can now use ColorBuffer in your code as any other StructuredBuffer.
#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

// Usage: StructuredBuffer<MyStruct> myStruct = VEX_GET_BINDLESS_RESOURCE(index);
// Can now use myStruct in your code as any other StructuredBuffer.
#define VEX_GET_BINDLESS_RESOURCE(index) ResourceDescriptorHeap[index];

)";