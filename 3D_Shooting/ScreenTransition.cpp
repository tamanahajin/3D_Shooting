#include "stdafx.h"
#include "ScreenTransition.h"

namespace shooting {

	namespace
	{
		float Clamp01(float value)
		{
			if (value < 0.0f) return 0.0f;
			if (value > 1.0f) return 1.0f;
			return value;
		}
	}

	void ScreenTransition::Start(
		float fadeOutSeconds,
		float fadeInSeconds,
		CoveredCallback onCovered,
		const D2D1_COLOR_F& color)
	{
		m_Phase = Phase::FadeOut;
		m_Timer = 0.0f;
		m_FadeOutSeconds = bsmUtil::Max(0.001f, fadeOutSeconds);
		m_FadeInSeconds = bsmUtil::Max(0.001f, fadeInSeconds);
		m_Alpha = 0.0f;
		m_Color = color;
		m_OnCovered = std::move(onCovered);
	}

	void ScreenTransition::Update(double elapsedTime)
	{
		if (m_Phase == Phase::None)
		{
			return;
		}

		m_Timer += static_cast<float>(elapsedTime);

		if (m_Phase == Phase::FadeOut)
		{
			m_Alpha = Clamp01(m_Timer / m_FadeOutSeconds);
			if (m_Timer < m_FadeOutSeconds)
			{
				return;
			}

			m_Alpha = 1.0f;
			if (m_OnCovered)
			{
				auto callback = std::move(m_OnCovered);
				m_OnCovered = nullptr;
				callback();
			}

			m_Phase = Phase::FadeIn;
			m_Timer = 0.0f;
			return;
		}

		if (m_Phase == Phase::FadeIn)
		{
			m_Alpha = 1.0f - Clamp01(m_Timer / m_FadeInSeconds);
			if (m_Timer >= m_FadeInSeconds)
			{
				Finish();
			}
		}
	}

	D2D1_COLOR_F ScreenTransition::GetOverlayColor() const
	{
		D2D1_COLOR_F color = m_Color;
		color.a *= m_Alpha;
		return color;
	}

	void ScreenTransition::Finish()
	{
		m_Phase = Phase::None;
		m_Timer = 0.0f;
		m_Alpha = 0.0f;
		m_OnCovered = nullptr;
	}

}
