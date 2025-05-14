#pragma once

#include <Vex/RHI/RHITexture.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Texture : public RHITexture
{
public:
    DX12Texture(ComPtr<DX12Device>& device, const TextureDescription& description);
    DX12Texture(std::string name, ComPtr<ID3D12Resource> rawTex);
    virtual ~DX12Texture() override;

    ID3D12Resource* GetRawTexture()
    {
        return texture.Get();
    }

private:
    ComPtr<ID3D12Resource> texture;
};

} // namespace vex::dx12