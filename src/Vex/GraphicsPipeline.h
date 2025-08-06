#pragma once

#include <array>
#include <string>
#include <vector>

#include <Vex/EnumFlags.h>
#include <Vex/Formats.h>
#include <Vex/Hash.h>
#include <Vex/Types.h>

namespace vex
{

struct VertexInputLayout
{
    struct VertexAttribute
    {
        std::string semanticName; // eg: "TEXCOORD", "POSITION", "NORMAL", etc...
        u32 semanticIndex;        // 0, 1, 2, etc...
        u32 binding;
        TextureFormat format;
        u32 offset;

        bool operator==(const VertexAttribute& other) const = default;
    };

    enum InputRate : u8
    {
        PerVertex,
        PerInstance,
    };

    struct VertexBinding
    {
        u32 binding;
        u32 stride;
        InputRate inputRate;

        bool operator==(const VertexBinding& other) const = default;
    };

    std::vector<VertexAttribute> attributes;
    std::vector<VertexBinding> bindings;

    bool operator==(const VertexInputLayout& other) const = default;
};

enum class InputTopology : u8
{
    TriangleList,
    TriangleStrip,
    TriangleFan,
};

struct InputAssembly
{
    InputTopology topology = InputTopology::TriangleList;
    bool primitiveRestartEnabled = false; // Vulkan only

    bool operator==(const InputAssembly& other) const = default;
};

enum class CullMode : u8
{
    None,
    Front,
    Back,
};

enum class PolygonMode : u8
{
    Fill,
    Line,  // Vulkan only
    Point, // Vulkan only
};

enum class FillMode : u8
{
    Wireframe,
    Solid,
};

enum class Winding : u8
{
    CounterClockwise,
    Clockwise,
};

struct RasterizerState
{
    bool rasterizerDiscardEnabled = false;
    bool depthClampEnabled = false; // Vulkan only
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::Back;
    Winding winding = Winding::CounterClockwise;
    bool depthBiasEnabled = false;
    float depthBiasConstantFactor = 0;
    float depthBiasClamp = 0;
    float depthBiasSlopeFactor = 0;
    float lineWidth = 0; // Vulkan only

    bool operator==(const RasterizerState& other) const = default;
};

// Note: for now Multisampling is unsupported in Vex. If the need ever arises it would be trivial to add.

// See this for an explanation of each operation:
// https://registry.khronos.org/vulkan/specs/latest/man/html/VkCompareOp.html#_description
// Mapping is 1:1 between DX12 and Vulkan (dx12 enum value is the vulkan enum value + 1).
enum class CompareOp : u8
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
    None = 99,
};

// See this for an explanation of each operation:
// https://registry.khronos.org/vulkan/specs/latest/man/html/VkStencilOp.html#_description
// Mapping is 1:1 between DX12 and Vulkan (dx12 enum value is the vulkan enum value + 1).
enum class StencilOp : u8
{
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

struct DepthStencilState
{
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    CompareOp depthCompareOp = CompareOp::None;
    bool depthBoundsTestEnabled = false; // Vulkan only
    bool stencilTestEnabled = false;     // Vulkan only
    struct StencilOpState
    {
        StencilOp failOp = StencilOp::Keep;
        StencilOp passOp = StencilOp::Keep;
        StencilOp depthFailOp = StencilOp::Keep;
        CompareOp compareOp = CompareOp::Always;
        u32 readMask = 0;  // Only u8 in DX12
        u32 writeMask = 0; // Only u8 in DX12
        u32 reference = 0; // Vulkan only

        bool operator==(const StencilOpState& other) const = default;
    };
    StencilOpState front;
    StencilOpState back;
    float minDepthBounds = 0; // Vulkan only
    float maxDepthBounds = 0; // Vulkan only

    bool operator==(const DepthStencilState& other) const = default;
};

// See the following link for an explanation of each step:
// https://registry.khronos.org/vulkan/specs/latest/man/html/VkLogicOp.html#_description
// This is a Vulkan-only concept.
enum class LogicOp : u8
{
    Clear,
    And,
    AndReverse,
    Copy,
    AndInverted,
    NoOp,
    Xor,
    Or,
    Nor,
    Equivalent,
    Invert,
    OrReverse,
    CopyInverted,
    OrInverted,
    Nand,
    Set,
};

// See the following link for more info:
// https://registry.khronos.org/vulkan/specs/latest/man/html/VkBlendFactor.html#_description
// This one has the same values in DX12 and Vulkan, but ordered differently...
enum class BlendFactor : u8
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    SrcAlphaSaturate,
    Src1Color,
    OneMinusSrc1Color,
    Src1Alpha,
    OneMinusSrc1Alpha,
};

// See the following link for more info:
// https://registry.khronos.org/vulkan/specs/latest/man/html/VkBlendOp.html#_description
// The mapping is 1:1 here between DX12 and Vulkan (dx12 enum = vulkan enum + 1).
enum class BlendOp : u8
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

// clang-format off

// Flags for which channels are written to the render target.
BEGIN_VEX_ENUM_FLAGS(ColorWriteMask, u8)
    None,
    Red = 1,
    Green = 2,
    Blue = 4,
    Alpha = 8,
    All = 0b1111,
END_VEX_ENUM_FLAGS();

// clang-format on

struct ColorBlendState
{
    bool logicOpEnabled = false;      // Vulkan only
    LogicOp logicOp = LogicOp::Clear; // Vulkan only

    struct ColorBlendAttachment
    {
        bool blendEnabled = false;
        BlendFactor srcColorBlendFactor = BlendFactor::One;
        BlendFactor dstColorBlendFactor = BlendFactor::Zero;
        BlendOp colorBlendOp = BlendOp::Add;
        BlendFactor srcAlphaBlendFactor = BlendFactor::One;
        BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
        BlendOp alphaBlendOp = BlendOp::Add;
        ColorWriteMask::Flags colorWriteMask = ColorWriteMask::All;

        bool operator==(const ColorBlendAttachment& other) const = default;
    };

    // One blend attachment per render target.
    std::vector<ColorBlendAttachment> attachments;
    std::array<float, 4> blendConstants{};

    bool operator==(const ColorBlendState& other) const = default;
};

struct RenderTargetState
{
    std::vector<TextureFormat> colorFormats;
    TextureFormat depthStencilFormat;

    bool operator==(const RenderTargetState& other) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::VertexInputLayout::VertexAttribute,
    VEX_HASH_COMBINE(seed, obj.binding);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.offset);
    VEX_HASH_COMBINE(seed, obj.semanticName);
    VEX_HASH_COMBINE(seed, obj.semanticIndex);
);

VEX_MAKE_HASHABLE(vex::VertexInputLayout::VertexBinding,
    VEX_HASH_COMBINE(seed, obj.binding);
    VEX_HASH_COMBINE(seed, obj.stride);
    VEX_HASH_COMBINE(seed, obj.inputRate);
);

VEX_MAKE_HASHABLE(vex::VertexInputLayout,
    VEX_HASH_COMBINE_CONTAINER(seed, obj.attributes);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.bindings);
);

VEX_MAKE_HASHABLE(vex::InputAssembly,
    VEX_HASH_COMBINE(seed, obj.topology);
    VEX_HASH_COMBINE(seed, obj.primitiveRestartEnabled);
);

VEX_MAKE_HASHABLE(vex::RasterizerState,
    VEX_HASH_COMBINE(seed, obj.rasterizerDiscardEnabled);
    VEX_HASH_COMBINE(seed, obj.depthClampEnabled);
    VEX_HASH_COMBINE(seed, obj.polygonMode);
    VEX_HASH_COMBINE(seed, obj.cullMode);
    VEX_HASH_COMBINE(seed, obj.winding);
    VEX_HASH_COMBINE(seed, obj.depthBiasEnabled);
    VEX_HASH_COMBINE(seed, obj.depthBiasConstantFactor);
    VEX_HASH_COMBINE(seed, obj.depthBiasClamp);
    VEX_HASH_COMBINE(seed, obj.depthBiasSlopeFactor);
    VEX_HASH_COMBINE(seed, obj.lineWidth);
);

VEX_MAKE_HASHABLE(vex::DepthStencilState::StencilOpState,
    VEX_HASH_COMBINE(seed, obj.failOp);
    VEX_HASH_COMBINE(seed, obj.passOp);
    VEX_HASH_COMBINE(seed, obj.depthFailOp);
    VEX_HASH_COMBINE(seed, obj.compareOp);
    VEX_HASH_COMBINE(seed, obj.readMask);
    VEX_HASH_COMBINE(seed, obj.writeMask);
    VEX_HASH_COMBINE(seed, obj.reference);
);

VEX_MAKE_HASHABLE(vex::DepthStencilState,
    VEX_HASH_COMBINE(seed, obj.depthTestEnabled);
    VEX_HASH_COMBINE(seed, obj.depthWriteEnabled);
    VEX_HASH_COMBINE(seed, obj.depthCompareOp);
    VEX_HASH_COMBINE(seed, obj.depthBoundsTestEnabled);
    VEX_HASH_COMBINE(seed, obj.front);
    VEX_HASH_COMBINE(seed, obj.back);
    VEX_HASH_COMBINE(seed, obj.minDepthBounds);
    VEX_HASH_COMBINE(seed, obj.maxDepthBounds);
);

VEX_MAKE_HASHABLE(vex::ColorBlendState::ColorBlendAttachment,
    VEX_HASH_COMBINE(seed, obj.blendEnabled);
    VEX_HASH_COMBINE(seed, obj.srcColorBlendFactor);
    VEX_HASH_COMBINE(seed, obj.dstColorBlendFactor);
    VEX_HASH_COMBINE(seed, obj.colorBlendOp);
    VEX_HASH_COMBINE(seed, obj.srcAlphaBlendFactor);
    VEX_HASH_COMBINE(seed, obj.dstAlphaBlendFactor);
    VEX_HASH_COMBINE(seed, obj.alphaBlendOp);
    VEX_HASH_COMBINE(seed, obj.colorWriteMask);
);

VEX_MAKE_HASHABLE(vex::ColorBlendState,
    VEX_HASH_COMBINE(seed, obj.logicOpEnabled);
    VEX_HASH_COMBINE(seed, obj.logicOp);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.attachments);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.blendConstants);
);

VEX_MAKE_HASHABLE(vex::RenderTargetState,
    VEX_HASH_COMBINE_CONTAINER(seed, obj.colorFormats);
    VEX_HASH_COMBINE(seed, obj.depthStencilFormat);
);

// clang-format on
