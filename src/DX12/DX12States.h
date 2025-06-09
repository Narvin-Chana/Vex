#pragma once

#include <Vex/RHI/RHITexture.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

D3D12_RESOURCE_STATES RHITextureStateToDX12State(RHITextureState::Flags state);

}