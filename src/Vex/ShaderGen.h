#pragma once

constexpr const char* ShaderGenMacros = R"(

// Temp until alex's MR is merged.
#define VEX_DX12

#ifdef VEX_DX12

#define VEX_GLOBAL_RESOURCE(type, name) static type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]

#elif VEX_VULKAN

template<typename T> 
T VexGetBindlessResource(uint index);

template<> 
Texture2D VexGetBindlessResource<Texture2D>(uint index)
{
    return TextureDescriptorHeap[index];
}

template <>
RWTexture2D VexGetBindlessResource<RWTexture2D>(uint index)
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