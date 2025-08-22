#pragma once

#if defined(VEX_VULKAN)
#define VEX_UNIFORMS(type, name) [[vk::push_constant]] type name;
#elif defined(VEX_DX12)
#define VEX_UNIFORMS(type, name) ConstantBuffer<type> name : register(b0);
#endif

#define GetBindlessResource(index) ResourceDescriptorHeap[index & 0x00FFFFFF];
