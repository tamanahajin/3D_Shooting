#include "WaveEffect.hlsli"

cbuffer WaveEffectDrawCB : register(b0)
{
	matrix WorldViewProjection;
	float4 DrawColor;
	float4 WaveTimeAmplitudeFrequencySpeed;
	float4 WaveDirectionEdgeStartEnd;
	float4 WaveShakeAxis;
};

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 tex : TEXCOORD0;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
};

PSInput main(VSInput input)
{
	WaveEffectParams waveParams;
	waveParams.time = WaveTimeAmplitudeFrequencySpeed.x;
	waveParams.amplitude = WaveTimeAmplitudeFrequencySpeed.y;
	waveParams.frequency = WaveTimeAmplitudeFrequencySpeed.z;
	waveParams.speed = WaveTimeAmplitudeFrequencySpeed.w;
	waveParams.phaseDirection = WaveDirectionEdgeStartEnd.xy;
	waveParams.edgeStart = WaveDirectionEdgeStartEnd.z;
	waveParams.edgeEnd = WaveDirectionEdgeStartEnd.w;

	float3 localPosition = ApplyWaveEffect(input.position, WaveShakeAxis.xyz, waveParams);

	PSInput output;
	output.position = mul(float4(localPosition, 1.0f), WorldViewProjection);
	output.color = DrawColor;
	return output;
}
