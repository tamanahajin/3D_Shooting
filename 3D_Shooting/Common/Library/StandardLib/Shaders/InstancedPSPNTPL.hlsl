Texture2D DiffuseTexture : register(t1);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 WorldPos : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target0
{
    float4 texColor = DiffuseTexture.Sample(LinearSampler, input.TexCoord);
    return texColor * input.Color;
}