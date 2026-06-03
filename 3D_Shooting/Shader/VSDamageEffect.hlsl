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

    o.svpos = mul(float4(posW, 1.0f), gViewProj);
    return o;
}
