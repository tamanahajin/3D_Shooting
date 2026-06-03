#ifndef MAX_BONES
#define MAX_BONES 200
#endif

cbuffer OutlineCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float gOutlineWidth;
    float gDamage;
    float2 _pad;
    float4 gBones[3 * MAX_BONES];
};

float4x4 GetBoneMatrix(uint index)
{
    float4 r0 = gBones[index * 3 + 0];
    float4 r1 = gBones[index * 3 + 1];
    float4 r2 = gBones[index * 3 + 2];

    return transpose(float4x4(
        r0,
        r1,
        r2,
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    ));
}
