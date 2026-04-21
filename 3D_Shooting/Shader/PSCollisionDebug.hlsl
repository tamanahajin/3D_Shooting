cbuffer cb0 : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
    float3x4 gWorldInverseTranspose;
    float4 gEyePosition;
    float4 gLightDirection[3];
    float4 gLightDiffuseColor[3];
    float4 gLightSpecularColor[3];
    float4 gEmissiveColor;
    float4 gSpecularColorAndPower;
    float4 gDiffuseColor;
    float4 gLightPos;
    float4 gEyePos;
    float4x4 gLightView;
    float4x4 gLightProjection;
    float4 gFogVector;
    float4 gFogColor;
    float4 gActiveFlg;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

float4 main() : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 0.35f);
}