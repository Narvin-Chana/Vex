#pragma once

#include <string_view>

namespace vex
{

constexpr std::string_view ShaderGenGeneralMacros = R"(

// GENERAL MACROS -------------------------

#if defined(VEX_VULKAN)

struct zzzZZZ___VEX_COMBINED_CONSTANTS_STRUCT
{
    uint globalBindlessIndex;
    zzzZZZ___VEX_LOCAL_CONSTANTS_STRUCT(userLocalConstants)
};

[[vk::push_constant]] ConstantBuffer<zzzZZZ___VEX_COMBINED_CONSTANTS_STRUCT> zzzZZZ___VEX_COMBINED_CONSTANTS_CB : register(b0);

#endif

)";

constexpr std::string_view ShaderGenVulkanLocalConstantsStructMacroName = "zzzZZZ___VEX_LOCAL_CONSTANTS_STRUCT";

constexpr std::string_view ShaderGenBindingMacros = R"(

// Define the internal structure for global bindless resources
#if defined(VEX_DX12)
// DX12 leverages root constant buffers to have the generated constants directly in slot b0.
ConstantBuffer<zzzZZZ___GeneratedConstants> zzzZZZ___GeneratedConstantsCB : register(b0);
#elif defined(VEX_VULKAN)
// Vulkan must do some trickery, as PushConstants must be defined only once.
static ConstantBuffer<zzzZZZ___GeneratedConstants> zzzZZZ___GeneratedConstantsCB = ResourceDescriptorHeap[zzzZZZ___VEX_COMBINED_CONSTANTS_CB.globalBindlessIndex];
#endif

// BINDING MACROS -------------------------

// Usage: VEX_GLOBAL_RESOURCE(StructuredBuffer<Colors>, ColorBuffer);
// Can now use ColorBuffer in your code as any other StructuredBuffer.
#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

// Usage: StructuredBuffer<MyStruct> myStruct = VEX_GET_BINDLESS_RESOURCE(index);
// Can now use myStruct in your code as any other StructuredBuffer.
#define VEX_GET_BINDLESS_RESOURCE(index) ResourceDescriptorHeap[index];

#if defined(VEX_DX12)
#define VEX_LOCAL_CONSTANTS(type, name) ConstantBuffer<type> name : register(b1);
#elif defined(VEX_VULKAN)
// Does nothing, will be preprocessor-replaced.
#define VEX_LOCAL_CONSTANTS(type, name)
#endif

)";

} // namespace vex
