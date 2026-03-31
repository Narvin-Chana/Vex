#pragma once

#if VEX_SPIRV
#define VEX_UNIFORMS(type, name) [[vk::push_constant]] type name
#elif VEX_DXIL
#define VEX_UNIFORMS(type, name) ConstantBuffer<type> name : register(b0)
#endif

#define GetBindlessResource(index) ResourceDescriptorHeap[index & 0x00FFFFFF]
#define GetBindlessSampler(index) SamplerDescriptorHeap[index & 0x00FFFFFF]

static const uint GInvalidBindlessHandle = ~0U;
