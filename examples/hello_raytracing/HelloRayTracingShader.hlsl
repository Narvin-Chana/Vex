#include <Vex.hlsli>

struct UniformStruct
{
    uint outputTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const RWTexture2D<float4> OutputTexture = GetBindlessResource(Uniforms.outputTextureHandle);

[shader("raygeneration")]
void RayGenMain()
{
    // Get the dispatch coordinates
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;

    // HLSL writes to the left side of the screen.
    if (launchIndex.x > launchDimensions.x / 2.0f)
    {
        return;
    }

    // Convert pixel coordinates to clip space
    float2 clipPos = float2((float(launchIndex.x) + 0.5f) * 2.0f / float(launchDimensions.x) - 1.0f,
                            1.0f - (float(launchIndex.y) + 0.5f) * 2.0f / float(launchDimensions.y));

    // Write to output
    OutputTexture[launchIndex] = float4(1.0f, clipPos, 1.0f);
}