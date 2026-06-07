#include "BcINCConstant.hlsli"
#include "BcINCStructs.hlsli"
#include "BcINCCommon.hlsli"
#include "BcINCLighting.hlsli"
#include "BcINCShadow.hlsli"

struct InstancedPSInput
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

float4 main(InstancedPSInput input) : SV_Target0
{
    float4 color = input.Color;

    if (Activeflags.y > 0)
    {
        color *= Texture.Sample(Sampler, input.TexCoord);
    }

    if (Activeflags.x > 0)
    {
        float3 eyeVector = normalize(EyePosition - input.WorldPos.xyz);
        float3 worldNormal = normalize(input.Normal);
        ColorPair lightResult = ComputeLights(eyeVector, worldNormal, Activeflags.x);
        color.rgb *= lightResult.Diffuse;
    }

    if (Activeflags.z > 0)
    {
        const float3 ambient = float3(0.7f, 0.7f, 0.7f);
        color = AddPixelShadow(color, ambient, input.Normal, input.LightRay, input.LightViewVec, input.LightSpacePos);
    }

    const float damage = saturate(input.Damage);
    color.rgb = lerp(color.rgb, float3(1.0f, 0.08f, 0.04f), damage);

    return color;
}
