#pragma once

#include "VkDescriptorPool.h"
#include "VkHeaders.h"

#include <Vex/RHI/RHIBuffer.h>
#include <Vex/Types.h>

#include <span>

namespace vex
{
struct BufferDescription;
namespace BufferUtil
{
::vk::AccessFlags2 GetAccessFlagsFromBufferState(RHIBufferState::Flags flags);
}
} // namespace vex

namespace vex::vk
{
struct VkStagingBuffer;
struct VkGPUContext;
class VkBuffer;
struct VkDirectBufferMemory;

class VkBuffer : public RHIBuffer
{
public:
    VkBuffer(VkGPUContext& ctx, const BufferDescription& desc);

    virtual UniqueHandle<RHIMappedBufferMemory> GetMappedMemory() override;

    BindlessHandle GetOrCreateBindlessIndex(VkGPUContext& ctx, VkDescriptorPool& descriptorPool);
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual RHIBuffer* GetStagingBuffer() override;
    ::vk::Buffer& GetBuffer();

private:
    ::vk::UniqueBuffer buffer;
    ::vk::UniqueDeviceMemory memory;
    UniqueHandle<VkBuffer> stagingBuffer;
    VkGPUContext& ctx;

    std::optional<BindlessHandle> bufferHandle;

    friend struct VkStagedBufferMemory;
    friend struct VkDirectBufferMemory;
};

struct VkDirectBufferMemory : RHIMappedBufferMemory
{
    VkDirectBufferMemory(VkGPUContext& ctx, ::vk::DeviceMemory memory, u32 size);
    virtual void SetData(std::span<const u8> data) override;
    ~VkDirectBufferMemory() override;

    VkGPUContext& ctx;
    ::vk::DeviceMemory memory;
    u32 size;
    u8* data;
};

struct VkStagedBufferMemory : RHIMappedBufferMemory
{
    VkStagedBufferMemory(VkGPUContext& ctx, VkBuffer& target);
    virtual void SetData(std::span<const u8> data) override;
    ~VkStagedBufferMemory() override;

    VkBuffer& buffer;
    VkDirectBufferMemory mapping;
};

} // namespace vex::vk
