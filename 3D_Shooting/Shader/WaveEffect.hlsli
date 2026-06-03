#ifndef WAVE_EFFECT_HLSLI
#define WAVE_EFFECT_HLSLI

struct WaveEffectParams
{
	float time;
	float amplitude;
	float frequency;
	float speed;
	float2 phaseDirection;
	float edgeStart;
	float edgeEnd;
};

float ComputeWaveEdgeMask(float3 localPosition, float edgeStart, float edgeEnd)
{
	// 中心まで大きく揺らすと円盤全体がただ上下するだけに見えるため、
	// ローカルXZの半径を使って外側ほど揺れを強くする。
	const float radius = length(localPosition.xz);
	const float edgeWidth = max(edgeEnd - edgeStart, 0.0001f);
	return saturate((radius - edgeStart) / edgeWidth);
}

float3 ApplyWaveEffect(float3 localPosition, float3 shakeAxis, WaveEffectParams params)
{
	// この関数は「頂点を揺らす」ことだけを担当する。
	// 色、アルファ、ワープホールらしさは呼び出し側のシェーダーや描画クラスで決める。
	const float2 phaseDir = params.phaseDirection / max(length(params.phaseDirection), 0.0001f);
	const float phaseBase = dot(localPosition.xz, phaseDir);
	const float wave = sin((phaseBase * params.frequency) + (params.time * params.speed)) * params.amplitude;
	const float edgeMask = ComputeWaveEdgeMask(localPosition, params.edgeStart, params.edgeEnd);
	const float3 axis = shakeAxis / max(length(shakeAxis), 0.0001f);
	return localPosition + (axis * wave * edgeMask);
}

#endif
