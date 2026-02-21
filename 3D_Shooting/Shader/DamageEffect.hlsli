cbuffer OutlineCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float gOutlineWidth; // モデル単位（例: 0.02）
    float gDamage; // 0..1
    float2 _pad;
};