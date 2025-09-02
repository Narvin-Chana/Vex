import Vex;

// Passing in uniforms via the entry point does NOT currently work with RT shaders.
// There is a slang proposal to address this: https://github.com/shader-slang/spec/blob/main/proposals/017-shader-record.md
// The proposal is design-validated, but still needs to be implemented in the language.
// Meanwhile we just put them in the global namespace which works well enough.

[shader("raygeneration")]
void RayGenMain(uniform uint outputTextureHandle)
{
    let OutputTexture = GetBindlessResource<RWTexture2D<float4>>(outputTextureHandle);

    // Get the dispatch coordinates
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;

    // Slang writes to the right side of the screen.
    if (launchIndex.x <= launchDimensions.x / 2.0f)
    {
        return;
    }

    // Convert pixel coordinates to clip space
    float2 clipPos = float2((float(launchIndex.x) + 0.5f) * 2.0f / float(launchDimensions.x) - 1.0f,
                            1.0f - (float(launchIndex.y) + 0.5f) * 2.0f / float(launchDimensions.y));

    // Write to output
    OutputTexture[launchIndex] = float4(0.7f, clipPos, 1.0f);
}