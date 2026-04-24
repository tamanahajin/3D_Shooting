#include "DamageEffect.hlsli"

struct VSInput
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 pos = input.Position;

    float4 skinned =
        mul(pos, GetBoneMatrix(input.BoneIndices.x)) * input.BoneWeights.x +
        mul(pos, GetBoneMatrix(input.BoneIndices.y)) * input.BoneWeights.y +
        mul(pos, GetBoneMatrix(input.BoneIndices.z)) * input.BoneWeights.z +
        mul(pos, GetBoneMatrix(input.BoneIndices.w)) * input.BoneWeights.w;

    float4 p = mul(skinned, gWorld);
    output.svpos = mul(p, gViewProj);

    return output;
}