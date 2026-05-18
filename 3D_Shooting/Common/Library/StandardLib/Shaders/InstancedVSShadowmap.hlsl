#include "BcINCConstant.hlsli"

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;

    float4 Matrix0 : MATRIX0;
    float4 Matrix1 : MATRIX1;
    float4 Matrix2 : MATRIX2;
    float4 Matrix3 : MATRIX3;
};

float4 main(VSInput input) : SV_POSITION
{
    float4x4 instanceWorld = float4x4(
        input.Matrix0,
        input.Matrix1,
        input.Matrix2,
        input.Matrix3
    );

    float4 worldPos = mul(float4(input.Position, 1.0f), instanceWorld);
    float4 lightSpacePos = mul(worldPos, LightView);
    lightSpacePos = mul(lightSpacePos, LightProjection);
    return lightSpacePos;
}