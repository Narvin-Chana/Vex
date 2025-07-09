#ifdef VEX_VULKAN
RWTexture2D<float4> BindlessTextures[] : register(u3);

RWTexture2D<float4> GetBindlessTexture(uint index)
{
    return BindlessTextures[index];
}
#endif

#ifdef VEX_DX12
RWTexture2D<float4> GetBindlessTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
#endif