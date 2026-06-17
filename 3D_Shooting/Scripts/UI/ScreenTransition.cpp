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
		m_phase = Phase::FadeOut;
		m_timer = 0.0f;
		m_fadeOutSeconds = bsmUtil::Max(0.001f, fadeOutSeconds);
		m_fadeInSeconds = bsmUtil::Max(0.001f, fadeInSeconds);
		m_alpha = 0.0f;
		m_color = color;
		m_onCovered = std::move(onCovered);
	}

	void ScreenTransition::Update(double elapsedTime)
	{
		if (m_phase == Phase::None)
		{
			return;
		}

		m_timer += static_cast<float>(elapsedTime);

		if (m_phase == Phase::FadeOut)
		{
			m_alpha = Clamp01(m_timer / m_fadeOutSeconds);
			if (m_timer < m_fadeOutSeconds)
			{
				return;
			}

			m_alpha = 1.0f;
			if (m_onCovered)
			{
				auto callback = std::move(m_onCovered);
				m_onCovered = nullptr;
				callback();
			}

			m_phase = Phase::FadeIn;
			m_timer = 0.0f;
			return;
		}

		if (m_phase == Phase::FadeIn)
		{
			m_alpha = 1.0f - Clamp01(m_timer / m_fadeInSeconds);
			if (m_timer >= m_fadeInSeconds)
			{
				Finish();
			}
		}
	}

	D2D1_COLOR_F ScreenTransition::GetOverlayColor() const
	{
		D2D1_COLOR_F color = m_color;
		color.a *= m_alpha;
		return color;
	}

	void ScreenTransition::Finish()
	{
		m_phase = Phase::None;
		m_timer = 0.0f;
		m_alpha = 0.0f;
		m_onCovered = nullptr;
	}

}
