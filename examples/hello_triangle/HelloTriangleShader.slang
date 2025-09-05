import Vex;

struct Colors
{
    float4 cols;
};

// Sourced from IQuilez SDF functions
float dot2(float2 v)
{
    return dot(v, v);
}

float sdf(float2 p)
{
    p.x = abs(p.x);

    if (p.y + p.x > 1.0)
        return sqrt(dot2(p - float2(0.25, 0.75))) - sqrt(2.0f) / 4.0f;
    return sqrt(min(dot2(p - float2(0.00, 1.00)), dot2(p - 0.5f * max(p.x + p.y, 0.0f)))) * sign(p.x - p.y);
}

struct UniformsStruct
{
    uint ColorBuffer;
    uint CommBuffer;
    uint OutputTexture;
};

// Global way of mapping local constant bindless handles to resources.
// 'uniform' keyword automatically uses the declaration order to access from local constants.
// ----
// HOWEVER, for Vulkan, global uniforms are not appended as push_constant, instead they are combined into a
// "default" constant buffer that is bound to the first free slot.
// So we have to extra explicit and add the push_constant attribute.
// When compiling for DX12, the attribute is ignored (unlike hlsl which errors out).
// Generally you should prefer using the shader entry point anyways in non-RT shaders as this also better isolates your code and 
// stops uniforms from "leaking" into other entry points.
[vk::push_constant] uniform UniformsStruct Uniforms;

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    let ColorBuffer = GetBindlessResource<ConstantBuffer<Colors>>(Uniforms.ColorBuffer);
    let CommBuffer = GetBindlessResource<RWStructuredBuffer<float4>>(Uniforms.CommBuffer);
    let OutputTexture = GetBindlessResource<RWTexture2D<float4>>(Uniforms.OutputTexture);

    uint width, height;
    OutputTexture.GetDimensions(width, height);

    // Slang writes to the right side of the screen.
    if (dtid.x <= width / 2.0f)
    {
        return;
    }

    // Convert pixel coordinates to normalized space for opengl-based sdf function (-1 to 1)
    float2 uv = float2(dtid.xy) / max(width, height).xx * 2 - 1;

    float4 color = (*ColorBuffer).cols;
    CommBuffer[0] = float4(1, 1, 1, 1) - color;

    uv.y += 0.25f;
    uv.y *= -1;
    uv /= 0.63f;

    if (sdf(uv) < 0)
    {
        OutputTexture[dtid.xy] = float4(1.0f - color.rgb, 1.0f);
    }
    else
    {
        float2 globalUV = float2(dtid.xy) / max(width, height);
        OutputTexture[dtid.xy] = float4(0.2f, 0.1f, 0.1f, 1.0f);
    }
}
