#pragma once

constexpr const char* ShaderGenGeneralMacros = R"(
#if defined(VEX_DX12)

#define VEX_LOCAL_CONSTANT

#elif defined(VEX_VULKAN)

#define VEX_LOCAL_CONSTANT [[vk::push_constant]]

#endif
)";

constexpr const char* ShaderGenBindingMacros = R"(
#if defined(VEX_DX12)

#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

#elif defined(VEX_VULKAN)

// TODO: this only works with float4, figure out how to get all types working without having to spam many functions.
RWTexture2D<float4> RWTextureDescriptorHeap[] : register(u3);

template<typename T>
T VexGetBindlessResource(uint index);

template<>
RWTexture2D<float4> VexGetBindlessResource(uint index)
{
    return RWTextureDescriptorHeap[index];
}

// etc... for other heap types

#define VEX_GLOBAL_RESOURCE(type, name) static type name = VexGetBindlessResource<type>(zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex)

#endif

// TODO(https://trello.com/c/bIVo8EP9): Users could also eventually get the bindless index (eg for storing a StructuredBuffer of materials, each material having bindless indices towards its textures such as albedo.)
// In this case they should use this macro to get the resource, this macro should contain additional checks to avoid using invalid handles.
#define VEX_BINDLESS_RESOURCE(type, name, index) 0

)";