#pragma once
#include "stdafx.h"

namespace shooting {

	class ScreenTransition
	{
	public:
		using CoveredCallback = std::function<void()>;

		void Start(
			float fadeOutSeconds,
			float fadeInSeconds,
			CoveredCallback onCovered,
			const D2D1_COLOR_F& color = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
		void Update(double elapsedTime);
		bool IsActive() const { return m_Phase != Phase::None; }
		bool IsInputBlocked() const { return IsActive(); }
		float GetAlpha() const { return m_Alpha; }
		D2D1_COLOR_F GetOverlayColor() const;

	private:
		enum class Phase
		{
			None,
			FadeOut,
			FadeIn
		};

		void Finish();

		Phase m_Phase = Phase::None;
		float m_Timer = 0.0f;
		float m_FadeOutSeconds = 0.0f;
		float m_FadeInSeconds = 0.0f;
		float m_Alpha = 0.0f;
		D2D1_COLOR_F m_Color = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
		CoveredCallback m_OnCovered;
	};

}
