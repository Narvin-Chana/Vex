#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/Logger.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

namespace vex
{

enum class RHITextureState : u8
{
    Common = 0,
    CopySource,
    CopyDest,
    ShaderResource,
    ShaderReadWrite,
    DepthRead,
    DepthWrite,
    RenderTarget,
    Present,
};

class RHITextureBase : public MappableResourceInterface
{
public:
    static inline void ValidateStateVersusQueueType(RHITextureState state, CommandQueueType queueType)
    {
        using enum RHITextureState;

        static constexpr RHITextureState CopyQueueMaxState = CopyDest;
        static constexpr RHITextureState ComputeQueueMaxState = ShaderReadWrite;
        static constexpr RHITextureState GraphicsQueueMaxState = Present;

        bool isValid = false;
        switch (queueType)
        {
        case CommandQueueType::Graphics:
            isValid = state <= GraphicsQueueMaxState;
            break;
        case CommandQueueType::Compute:
            isValid = state <= ComputeQueueMaxState;
            break;
        case CommandQueueType::Copy:
            isValid = state <= CopyQueueMaxState;
            break;
        }

        if (!isValid)
        {
            VEX_LOG(Fatal,
                    "Unsupported transition state versus CommandQueue type: Cannot transition texture to state: {} "
                    "from queue type: {}.",
                    magic_enum::enum_name(state),
                    magic_enum::enum_name(queueType));
        }
    }

    RHITextureBase() = default;
    RHITextureBase(RHIAllocator& allocator)
        : allocator{ &allocator } {};
    RHITextureBase(const RHITextureBase&) = delete;
    RHITextureBase& operator=(const RHITextureBase&) = delete;
    RHITextureBase(RHITextureBase&&) = default;
    RHITextureBase& operator=(RHITextureBase&&) = default;
    ~RHITextureBase() = default;

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeAllocation(RHIAllocator& allocator) = 0;

    virtual RHITextureState GetClearTextureState() = 0;

    const TextureDescription& GetDescription() const
    {
        return description;
    }

    [[nodiscard]] RHITextureState GetCurrentState() const
    {
        return currentState;
    }

    void SetCurrentState(RHITextureState newState)
    {
        currentState = newState;
    }

    const Allocation& GetAllocation() const noexcept
    {
        return allocation;
    }

protected:
    TextureDescription description;
    RHITextureState currentState = RHITextureState::Common;

    RHIAllocator* allocator{};
    Allocation allocation;
};

} // namespace vex