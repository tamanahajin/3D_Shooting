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

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 WorldPos : TEXCOORD2;
    float Damage : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4x4 instanceWorld = float4x4(
        input.Matrix0,
        input.Matrix1,
        input.Matrix2,
        input.Matrix3
    );

    float4 localPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(localPos, instanceWorld);

    output.Position = mul(worldPos, WorldViewProj);
    output.Color = DiffuseColor;
    output.TexCoord = input.TexCoord;
    output.Normal = normalize(mul(input.Normal, (float3x3) instanceWorld));
    output.WorldPos = worldPos;
    output.Damage = 0.0f;

    return output;
}