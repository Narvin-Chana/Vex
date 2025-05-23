#include "DX12Texture.h"

#include <Vex/Logger.h>
#include <Vex/Platform/Windows/WString.h>

#include <DX12/DX12Formats.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, const TextureDescription& desc)
{
    description = desc;

    // TODO: implement texture creation from texture description.
    // chk << device->CreateCommittedResource(, , , , , IID_PPV_ARGS(&texture));
    VEX_NOT_YET_IMPLEMENTED();
}

DX12Texture::DX12Texture(std::string name, ComPtr<ID3D12Resource> nativeTex)
    : texture(std::move(nativeTex))
{
    VEX_ASSERT(texture, "The texture passed in should be defined!");
    description.name = std::move(name);
    D3D12_RESOURCE_DESC nativeDesc = texture->GetDesc();
    if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
        // Array size of 6 and TEXTURE2D resource dimension means we suppose the texture is a cubemap.
        if (nativeDesc.DepthOrArraySize == 6)
        {
            description.type = TextureType::TextureCube;
        }
        else
        {
            description.type = TextureType::Texture2D;
        }
    }
    else if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
        description.type = TextureType::Texture3D;
    }
    else
    {
        VEX_LOG(Fatal, "Vex DX12 RHI does not support 1D textures.");
        return;
    }
    description.width = nativeDesc.Width;
    description.height = nativeDesc.Height;
    description.depthOrArraySize = static_cast<u32>(nativeDesc.DepthOrArraySize);
    description.mips = nativeDesc.MipLevels;
    description.format = DXGIToTextureFormat(nativeDesc.Format);

    texture->SetName(StringToWString(description.name).data());
}

DX12Texture::~DX12Texture()
{
}

} // namespace vex::dx12