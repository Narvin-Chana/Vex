#include <Vex.hlsli>

struct UniformStruct
{
    float time;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(in float3 position : POSITION)
{
    VSOutput vs;

    float timeScale = Uniforms.time * 0.5f;
    
    float cosY = cos(timeScale);
    float sinY = sin(timeScale);
    float3x3 rotationY = float3x3(
        cosY,  0.0f, sinY,
        0.0f,  1.0f, 0.0f,
        -sinY, 0.0f, cosY
    );
    
    float cosX = cos(timeScale * 0.7f);
    float sinX = sin(timeScale * 0.7f);
    float3x3 rotationX = float3x3(
        1.0f, 0.0f,  0.0f,
        0.0f, cosX, -sinX,
        0.0f, sinX,  cosX
    );

    float scale = 0.5f;
    float3x3 scaleMatrix = float3x3(
        scale, 0, 0,
        0, scale, 0,
        0, 0, scale
    );
    
    float3 scaledPosition = mul(scaleMatrix, position);
    
    float3x3 finalRotation = mul(rotationY, rotationX);
    float3 rotatedPosition = mul(finalRotation, scaledPosition);
    
    float3 finalPosition = rotatedPosition + float3(-0.4f, 0.3f, 0);
    
    vs.position = float4(finalPosition, 1);

    return vs;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return abs(input.position.xyzw);
}
