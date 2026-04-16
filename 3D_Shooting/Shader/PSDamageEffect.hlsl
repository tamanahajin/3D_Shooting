#include "DamageEffect.hlsli"

float4 main() : SV_TARGET
{
	// damage‚Å”Z‚³‚ğ’²®i“_–Å‚³‚¹‚½‚¢‚È‚ç time ‚ğ“n‚µ‚Ä sin ‚È‚Çj
    return float4(1.0f, 0.0f, 0.0f, saturate(gDamage) * 0.5);
    // return float4(0.0f, 1.0f, 0.0f, 1.0f);
}