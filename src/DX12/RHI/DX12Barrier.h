#pragma once

#include <RHI/RHIBarrier.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

D3D12_BARRIER_SYNC RHIBarrierSyncToDX12(RHIBarrierSync barrierSync);
D3D12_BARRIER_ACCESS RHIBarrierAccessToDX12(RHIBarrierAccess barrierAccess);
D3D12_BARRIER_LAYOUT RHITextureLayoutToDX12(RHITextureLayout textureLayout);

} // namespace vex::dx12