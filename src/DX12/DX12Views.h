#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

struct DX12TextureView : TextureView
{
    DXGI_FORMAT format;
};

struct DX12BufferView : BufferView
{
    // TODO: implement buffers
};

} // namespace vex::dx12