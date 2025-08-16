struct MyStruct
{
    float time;
};

struct Colors
{
    float4 cols;
};

VEX_SHADER
{
    VEX_LOCAL_CONSTANTS(
        MyStruct,
        MyLocalConstants
    );
    VEX_GLOBAL_RESOURCE(ConstantBuffer<Colors>, ColorBuffer);
}

void ComputeTriangleVertex(in uint vertexID, out float4 position, out float2 uv)
{
    uv = float2((vertexID << 1) & 2, vertexID & 2);
    position = float4(uv.x * 2 - 1, -uv.y * 2 + 1, 0, 1);
}

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(in uint vertexID : SV_VertexID)
{
    VSOutput vs;
    ComputeTriangleVertex(2 - vertexID, vs.pos, vs.uv);
    return vs;
}

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

float4 PSMain(VSOutput input)
    : SV_Target
{
    static const float moveSpeed = 0.2f;
    float offset = sin(MyLocalConstants.time * moveSpeed) * 0.1;

    // Define triangle vertices in normalized space
    float2 v0 = float2(0.5 + offset, 0.1); // Bottom center
    float2 v1 = float2(0.9 + offset, 0.8); // Top right
    float2 v2 = float2(0.1 + offset, 0.8); // Top left

    if (IsInsideTriangle(input.uv, v0, v1, v2))
    {
        float3 color = float3(input.uv.x, input.uv.y, 1 - input.uv.x * input.uv.y);
        return float4(color * ColorBuffer.cols.rgb, 1);
    }
    else
    {
        return float4(0, 0, 0, 1);
    }
}
