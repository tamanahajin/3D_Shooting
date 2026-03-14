#include "DamageEffect.hlsli"

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
};

struct VSOut
{
    float4 svpos : SV_POSITION;
};

VSOut main(VSIn i)
{
    VSOut o;

    float3 posW = mul(float4(i.pos, 1.0f), gWorld).xyz;

    // ※法線はワールドへ（非一様スケールがあるなら逆転置を使うのが理想）
    // float3 nW = normalize(mul(float4(i.nrm, 0.0f), gWorld).xyz);

    // ダメージ中だけ膨らむ
    // posW += nW * (gOutlineWidth * saturate(gDamage));

    o.svpos = mul(float4(posW, 1.0f), gViewProj);
    return o;
}