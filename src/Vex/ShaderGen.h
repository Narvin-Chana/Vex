#pragma once

constexpr const char* ShaderGenMacros = R"(

// Temp until I rebase
#define VEX_DX12

#ifdef VEX_DX12

#define VEX_RESOURCE(type, name) type name = ResourceDescriptorHeap[zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex]
#define VEX_GLOBAL_RESOURCE(type, name) static VEX_RESOURCE(type, name)

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

#define VEX_RESOURCE(type, name) type name = VexGetBindlessResource<type>(zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex)
#define VEX_GLOBAL_RESOURCE(type, name) static VEX_RESOURCE(type, name)

#endif

)";