#include "DamageEffect.hlsli"

float4 main() : SV_TARGET
{
	// damageで濃さを調整（点滅させたいなら time を渡して sin など）
    return float4(1.0f, 0.0f, 0.0f, saturate(gDamage) * 0.8);
    //return float4(1.0f, 0.0f, 0.0f, 1.0f);
}