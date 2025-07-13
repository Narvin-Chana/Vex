#ifdef VEX_VULKAN

#define PUSH_CONSTANTS [[vk::push_constant]]

RWTexture2D<float4> BindlessTextures[] : register(u3);

RWTexture2D<float4> GetBindlessTexture(uint index)
{
    return BindlessTextures[index];
}
#endif

#ifdef VEX_DX12

#define PUSH_CONSTANTS

RWTexture2D<float4> GetBindlessTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
#endif