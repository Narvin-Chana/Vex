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

#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

// TODO(https://trello.com/c/bIVo8EP9): Users could also eventually get the bindless index (eg for storing a StructuredBuffer of materials, each material having bindless indices towards its textures such as albedo.)
// In this case they should use this macro to get the resource, this macro should contain additional checks to avoid using invalid handles (should it really? validation layers have become really good at detecting invalid usage of descriptors...).
#define VEX_GET_BINDLESS_RESOURCE(type, index) ResourceDescriptorHeap[index];

)";