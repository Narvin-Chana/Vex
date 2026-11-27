#pragma once

#include <concepts>
#include <utility>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

// Holds a native object that must stay alive until in-flight GPU work referencing it has completed.
// We can just use ID3D12DeviceChild since all d3d12 objects are derived from it.
class DX12DeferredRelease
{
public:
    template <class T>
        requires std::derived_from<T, ID3D12DeviceChild>
    explicit DX12DeferredRelease(ComPtr<T>&& object)
        : object(std::move(object))
    {
    }

    void Release()
    {
        object.Reset();
    }

private:
    ComPtr<ID3D12DeviceChild> object;
};

} // namespace vex::dx12
