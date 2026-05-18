#include "BcINCConstant.hlsli"

StructuredBuffer<float4> InstanceBoneRows : register(t2);

struct VSInput
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4 Indices : BLENDINDICES0;
    float4 Weights : BLENDWEIGHT0;

    float4 Matrix0 : MATRIX0;
    float4 Matrix1 : MATRIX1;
    float4 Matrix2 : MATRIX2;
    float4 Matrix3 : MATRIX3;
    float4 InstanceParam : TEXCOORD1;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 WorldPos : TEXCOORD2;
    float Damage : TEXCOORD3;
    float4 LightSpacePos : TEXCOORD4;
    float3 LightRay : TEXCOORD5;
    float3 LightViewVec : TEXCOORD6;
};

float4x3 LoadBone(uint boneIndex)
{
    uint row = boneIndex * 3;
    float4 c0 = InstanceBoneRows[row + 0];
    float4 c1 = InstanceBoneRows[row + 1];
    float4 c2 = InstanceBoneRows[row + 2];

    return float4x3(
        c0.x, c1.x, c2.x,
        c0.y, c1.y, c2.y,
        c0.z, c1.z, c2.z,
        c0.w, c1.w, c2.w);
}

VSOutput main(VSInput input)
{
    VSOutput output;

    float4x4 instanceWorld = float4x4(
        input.Matrix0,
        input.Matrix1,
        input.Matrix2,
        input.Matrix3);

    uint boneStart = (uint)(input.InstanceParam.x + 0.5f);

    float4x3 skinning = (float4x3)0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        skinning += LoadBone(boneStart + input.Indices[i]) * input.Weights[i];
    }

    float3 skinnedPos = mul(input.Position, skinning);
    float3 skinnedNormal = mul(input.Normal, (float3x3)skinning);

    float4 worldPos = mul(float4(skinnedPos, 1.0f), instanceWorld);

    output.Position = mul(worldPos, WorldViewProj);
    output.Color = DiffuseColor;
    output.TexCoord = input.TexCoord;
    output.Normal = normalize(mul(skinnedNormal, (float3x3)instanceWorld));
    output.WorldPos = worldPos;
    output.Damage = saturate(input.InstanceParam.y);
    output.LightSpacePos = mul(worldPos, LightView);
    output.LightSpacePos = mul(output.LightSpacePos, LightProjection);
    output.LightRay = LightPos.xyz - worldPos.xyz;
    output.LightViewVec = EyePos.xyz - worldPos.xyz;

    return output;
}
