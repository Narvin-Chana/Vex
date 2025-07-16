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

// Heaps can overlap in vulkan! This means we can just declare every heap with every type, 
// in order to have a singular templated function to access any bindless resource.

#define VEX_DECLARE_DESCRIPTOR_HEAP(type, primType, name, REG) \
    type<primType> name##_##primType[] : register(REG);

#define VEX_DECLARE_DESCRIPTOR_HEAP_FOR_EACH_PRIMITIVE(type, name, REG) \
    VEX_DECLARE_DESCRIPTOR_HEAP(type, float4, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, float3, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, float2, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, float, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, uint4, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, uint3, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, uint2, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, uint, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, int4, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, int3, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, int2, name, REG)\
    VEX_DECLARE_DESCRIPTOR_HEAP(type, int, name, REG)

template<typename T>
T VexGetBindlessResource(uint index);

#define VEX_RESOURCE_BINDLESS_DECLARATION(type, primType, heapName) \
    template<>\
    type<primType> VexGetBindlessResource(uint index)\
    {\
        return heapName##_##primType[index];\
    }

#define VEX_RESOURCE_BINDLESS_DECLARATION_FOR_EACH_PRIMITIVE(type, heapName) \
    VEX_RESOURCE_BINDLESS_DECLARATION(type, float4, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, float3, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, float2, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, float, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, uint4, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, uint3, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, uint2, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, uint, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, int4, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, int3, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, int2, heapName)\
    VEX_RESOURCE_BINDLESS_DECLARATION(type, int, heapName)

// -----------------------------------------------------------------------------------------------

VEX_DECLARE_DESCRIPTOR_HEAP_FOR_EACH_PRIMITIVE(RWTexture2D, RWTexture2DDescriptorHeap, u3)
VEX_RESOURCE_BINDLESS_DECLARATION_FOR_EACH_PRIMITIVE(RWTexture2D, RWTexture2DDescriptorHeap)

// etc... for other heap types

// -----------------------------------------------------------------------------------------------

#undef VEX_DECLARE_DESCRIPTOR_HEAP
#undef VEX_DECLARE_DESCRIPTOR_HEAP_FOR_EACH_PRIMITIVE
#undef VEX_RESOURCE_BINDLESS_DECLARATION_FOR_EACH_PRIMITIVE
#undef VEX_RESOURCE_BINDLESS_DECLARATION

// -----------------------------------------------------------------------------------------------

#define VEX_GLOBAL_RESOURCE(type, name) static type name = VexGetBindlessResource<type>(zzzZZZ___GeneratedConstantsCB.name##_bindlessIndex)

#endif

// TODO(https://trello.com/c/bIVo8EP9): Users could also eventually get the bindless index (eg for storing a StructuredBuffer of materials, each material having bindless indices towards its textures such as albedo.)
// In this case they should use this macro to get the resource, this macro should contain additional checks to avoid using invalid handles.
#define VEX_BINDLESS_RESOURCE(type, name, index) 0

)";