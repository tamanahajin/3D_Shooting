#define MAX_BONES 200

cbuffer ShadowCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProjection;
    float4 gBones[3 * MAX_BONES];
};

struct VSInput
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

float4x4 GetBoneMatrix(uint index)
{
    float4 r0 = gBones[index * 3 + 0];
    float4 r1 = gBones[index * 3 + 1];
    float4 r2 = gBones[index * 3 + 2];

    // C++側は bone 行列を 3 行分の Vec4 として詰めているため、
    // HLSL側で transpose して通常の行列向きに戻す
    return transpose(float4x4(
        r0,
        r1,
        r2,
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    ));
}

float4 main(VSInput input) : SV_POSITION
{
    float4 pos = input.Position;

    float4 skinned =
        mul(pos, GetBoneMatrix(input.BoneIndices.x)) * input.BoneWeights.x +
        mul(pos, GetBoneMatrix(input.BoneIndices.y)) * input.BoneWeights.y +
        mul(pos, GetBoneMatrix(input.BoneIndices.z)) * input.BoneWeights.z +
        mul(pos, GetBoneMatrix(input.BoneIndices.w)) * input.BoneWeights.w;

    float4 p = mul(skinned, gWorld);
    p = mul(p, gView);
    p = mul(p, gProjection);

    return p;
}