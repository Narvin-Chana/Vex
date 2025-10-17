#pragma once

#include <format>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Debug.h>
#include <Vex/Platform/Platform.h>
#include <Vex/Types.h>

#include <DX12/DX12Headers.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

enum class DX12HeapType : u8
{
    CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    SAMPLER = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
    RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
};

enum class HeapFlags : u8
{
    NONE = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    SHADER_VISIBLE = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
};

template <DX12HeapType Type, HeapFlags Flags = HeapFlags::NONE>
class DX12DescriptorHeap
{
    using enum HeapFlags;
    using enum DX12HeapType;

public:
    DX12DescriptorHeap() = default;
    DX12DescriptorHeap(ComPtr<DX12Device>& device, u32 heapSize, const std::string& name)
        : size{ heapSize }
    {
        // These are in this constructor because there is a small compilation speed gain when not directly in a
        // class (evaluated less frequently).
        static_assert(!(Type == RTV && Flags == SHADER_VISIBLE), "Cannot have a shader visible RTV descriptor heap.");
        static_assert(!(Type == DSV && Flags == SHADER_VISIBLE), "Cannot have a shader visible DSV descriptor heap.");

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = size;
        heapDesc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(Type);
        heapDesc.Flags = static_cast<D3D12_DESCRIPTOR_HEAP_FLAGS>(Flags);
        heapDesc.NodeMask = 0;
        chk << device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));

        descriptorByteSize = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(Type));
#if !VEX_SHIPPING
        chk << heap->SetName(StringToWString(std::format("DescriptorHeap: {} {} ({})",
                                                         name,
                                                         magic_enum::enum_name(Type),
                                                         magic_enum::enum_name(Flags)))
                                 .c_str());
#endif
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(u32 index) const
    {
        VEX_ASSERT(index < size, "Trying to access index outside of descriptor heap subresource.");
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(heap->GetCPUDescriptorHandleForHeapStart(), index, descriptorByteSize);
        return handle;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(u32 index) const
    {
        static_assert(Flags == SHADER_VISIBLE);
        VEX_ASSERT(index < size, "Trying to access index outside of descriptor heap subresource.");
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle(heap->GetGPUDescriptorHandleForHeapStart(), index, descriptorByteSize);
        return handle;
    }

    ComPtr<ID3D12DescriptorHeap>& GetNativeDescriptorHeap()
    {
        return heap;
    }

private:
    // Number of descriptors able to be used in this heap.
    u32 size;
    // Byte size of one descriptor.
    u32 descriptorByteSize;
    ComPtr<ID3D12DescriptorHeap> heap;
};

} // namespace vex::dx12