#pragma once
#include "stdafx.h"

namespace shooting {

	class ExplosionEffect : public GameObject
	{
	private:
		float m_LifeTime = 0.45f;
		float m_Elapsed = 0.0f;
		float m_StartScale = 0.2f;
		float m_EndScale = 2.2f;
		float m_MaxAlpha = 0.95f;
		std::wstring m_TextureKey = L"EXPLOSION_FIRE_TX";

	public:
		explicit ExplosionEffect(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
			: GameObject(stagePtr)
		{
			m_transParam = param;
		}

		virtual ~ExplosionEffect() = default;

		void SetLifeTime(float v) { m_LifeTime = v; }
		void SetScaleRange(float startScale, float endScale)
		{
			m_StartScale = startScale;
			m_EndScale = endScale;
		}
		void SetTextureKey(const std::wstring& key) { m_TextureKey = key; }

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
	};

}
