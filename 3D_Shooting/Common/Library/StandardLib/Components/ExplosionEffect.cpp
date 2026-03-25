#include "stdafx.h"
#include "Project.h"
#include "ExplosionEffect.h"

namespace shooting {

	namespace
	{
		float Saturate(float v)
		{
			return bsmUtil::Max(0.0f, bsmUtil::Min(v, 1.0f));
		}

		float SmoothStep(float t)
		{
			t = Saturate(t);
			return t * t * (3.0f - 2.0f * t);
		}
	}

	void ExplosionEffect::OnCreate()
	{
		AddTag(L"ExplosionEffect");
		SetAlphaActive(true);
		SetDrawActive(true);
		SetUpdateActive(true);

		auto draw = AddComponent<SpPNTStaticDraw>();
		draw->AddBaseMesh(L"DEFAULT_SPHERE");
		draw->AddBaseTexture(m_TextureKey);
		draw->SetOwnShadowActive(false);
		draw->SetEmissive(Col4(1.4f, 0.9f, 0.35f, 1.0f));
		draw->SetDiffuse(Col4(1.0f, 0.7f, 0.25f, m_MaxAlpha));
		draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));

		if (auto tr = GetComponent<Transform>())
		{
			tr->SetScale(Vec3(m_StartScale, m_StartScale, m_StartScale));
		}
	}

	void ExplosionEffect::OnUpdate(double elapsedTime)
	{
		m_Elapsed += static_cast<float>(elapsedTime);

		const float rawT = (m_LifeTime > 0.0f) ? (m_Elapsed / m_LifeTime) : 1.0f;
		const float t = Saturate(rawT);
		const float s = bsmUtil::Lerp(m_StartScale, m_EndScale, SmoothStep(t));

		if (auto tr = GetComponent<Transform>())
		{
			tr->SetScale(Vec3(s, s, s));
		}

		if (auto draw = GetComponent<SpPNTStaticDraw>(false))
		{
			const float alpha = (1.0f - t) * m_MaxAlpha;
			const float glow = 1.2f - (0.5f * t);
			draw->SetEmissive(Col4(glow, glow * 0.7f, glow * 0.25f, 1.0f));
			draw->SetDiffuse(Col4(1.0f, 0.7f - (0.2f * t), 0.2f, alpha));
		}

		if (m_Elapsed >= m_LifeTime)
		{
			if (auto stage = GetStage(false))
			{
				stage->RemoveGameObject(GetThis<GameObject>());
			}
		}
	}

}
