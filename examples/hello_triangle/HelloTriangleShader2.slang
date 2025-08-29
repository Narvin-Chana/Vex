import Vex;

// Simple function to check if a point is inside a triangle
bool IsInsideTriangle(float2 p, float2 v0, float2 v1, float2 v2)
{
    float2 e0 = v1 - v0;
    float2 e1 = v2 - v1;
    float2 e2 = v0 - v2;

    float2 p0 = p - v0;
    float2 p1 = p - v1;
    float2 p2 = p - v2;

    float c0 = p0.x * e0.y - p0.y * e0.x;
    float c1 = p1.x * e1.y - p1.y * e1.x;
    float c2 = p2.x * e2.y - p2.y * e2.x;

    return (c0 >= 0 && c1 >= 0 && c2 >= 0) || (c0 <= 0 && c1 <= 0 && c2 <= 0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(
    uint3 dtid : SV_DispatchThreadID,
    // Uniforms passed in as entrypoint parameters are automatically appended as local constants by Slang!
    uniform uint OutputTextureHandle,
    uniform uint CommBufferHandle,
    uniform uint SourceTextureHandle
)
{
    let OutputTexture = GetBindlessResource<RWTexture2D<float4>>(OutputTextureHandle); 
    let CommBuffer = GetBindlessResource<StructuredBuffer<float4>>(CommBufferHandle);
    let SourceTexture = GetBindlessResource<Texture2D<float4>>(SourceTextureHandle);

    uint width, height;
    OutputTexture.GetDimensions(width, height);

    // Slang writes to the right side of the screen.
    if (dtid.x <= width / 2.0f)
    {
        return;
    }

    // Convert pixel coordinates to normalized space (0 to 1)
    float2 uv = float2(dtid.xy) / min(width, height).xx;

    // Define triangle vertices in normalized space
    float2 v0 = float2(0.5, 0.1); // Bottom center
    float2 v1 = float2(0.9, 0.8); // Top right
    float2 v2 = float2(0.1, 0.8); // Top left

    if (IsInsideTriangle(uv, v0, v1, v2) || IsInsideTriangle(uv - float2(1.15, 0), v0, v1, v2))
    {
        float3 color = float3(uv.x, uv.y, 1 - uv.x * uv.y);
        OutputTexture[dtid.xy] = float4(color, 1) * CommBuffer[0];
    }
    else
    {
        OutputTexture[dtid.xy] = SourceTexture.Load(uint3(dtid.xy, 0));
    }
}
