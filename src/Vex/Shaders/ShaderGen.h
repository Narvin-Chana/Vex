#pragma once

#include <string_view>

namespace vex
{

constexpr std::string_view ShaderGenBindingMacros = R"(

// Vex Helper Macros -------------------------

// Usage: StructuredBuffer<MyStruct> myStruct = VEX_GET_BINDLESS_RESOURCE(index);
// Can now use myStruct in your code as any other StructuredBuffer.
#define VEX_GET_BINDLESS_RESOURCE(index) ResourceDescriptorHeap[index];

)";

} // namespace vex
