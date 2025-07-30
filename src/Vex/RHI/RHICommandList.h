#pragma once

#include <array>
#include <concepts>
#include <span>
#include <type_traits>
#include <utility>

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/RHI/RHITexture.h>
#include <Vex/Types.h>

namespace vex
{
struct ConstantBinding;
struct ResourceBinding;
class RHITexture;
class RHIBuffer;
struct RHITextureBinding;
struct RHIBufferBinding;
struct InputAssembly;

template <class T>
concept RHICommandListInterface = requires(T t, const T ct) {
    { ct.IsOpen() } -> std::same_as<bool>;

    t.Open();
    t.Close();

    t.SetViewport(float{}, // x
                  float{}, // y
                  float{}, // width
                  float{}, // height
                  float{}, // minDepth
                  float{}  // maxDepth
    );
    t.SetScissor(i32{}, // x
                 i32{}, // y
                 u32{}, // width
                 u32{}  // height
    );

    t.SetPipelineState(std::declval<const RHIGraphicsPipelineState&>() // graphicsPipelineState
    );
    t.SetPipelineState(std::declval<const RHIComputePipelineState&>() // computePipelineState
    );

    t.SetLayout(std::declval<RHIResourceLayout&>());
    t.SetLayoutLocalConstants(std::declval<const RHIResourceLayout&>(), // layout
                              std::span<const ConstantBinding>{}        // constants
    );
    t.SetLayoutResources(std::declval<const RHIResourceLayout&>(), // layout
                         std::span<RHITextureBinding>{},           // textures
                         std::span<RHIBufferBinding>{},            // buffers
                         std::declval<RHIDescriptorPool&>()        // descriptorPool
    );

    t.SetDescriptorPool(std::declval<RHIDescriptorPool&>(), // descriptorPool
                        std::declval<RHIResourceLayout&>()  // resourceLayout
    );
    t.SetInputAssembly(std::declval<InputAssembly>() // inputAssembly
    );

    t.ClearTexture(std::declval<RHITexture&>(),             // rhiTexture
                   std::declval<const ResourceBinding&>(),  // clearBinding
                   std::declval<const TextureClearValue&>() // clearValue
    );

    t.Transition(std::declval<RHITexture&>(), // texture
                 RHITextureState::Flags{}     // newState
    );
    t.Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>>{} // textureNewStatePairs
    );

    t.Draw(u32{} // vertexCount
    );
    t.Dispatch(std::declval<const std::array<u32, 3>&>() // groupCount
    );

    { ct.GetType() } -> std::same_as<CommandQueueType>;
};

} // namespace vex