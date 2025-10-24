import Vex;

static const int blurSize = 3;

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uniform uint inputTextureHandle, uniform uint outputTextureHandle)
{
    let InputTexture = GetBindlessResource<Texture2D<float4>>(inputTextureHandle);
    let OutputTexture = GetBindlessResource<RWTexture2D<float4>>(outputTextureHandle);

    uint width, height;
    InputTexture.GetDimensions(width, height);

    float4 value = float4(0, 0, 0, 0);

    for (int y = -blurSize; y <= blurSize; ++y) {
        for (int x = -blurSize; x <= blurSize; ++x) {
            int2 coord = (int2)dtid.xy + int2(x, y);
            if (coord.x < 0 || coord.x >= width || coord.y < 0 || coord.y >= height)
            {
                continue;
            }

            value += float4(InputTexture[coord].rgb, 1);
        }
    }

    OutputTexture[dtid.xy / 2] = float4(value.rgb / value.a, 1);
}
