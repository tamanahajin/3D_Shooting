// HitStopController.h
#pragma once

namespace shooting {

	class HitStopController
	{
	private:
		double m_timer = 0.0;
		double m_timeScale = 1.0;
		bool m_requestedThisFrame = false;

	public:
		void Request(double duration, double timeScale)
		{
			if (duration <= 0.0)
			{
				return;
			}

			timeScale = bsmUtil::Clamp(timeScale, 0.0, 1.0);
			m_timer = bsmUtil::Max(m_timer, duration);
			m_timeScale = bsmUtil::Min(m_timeScale, timeScale);
			m_requestedThisFrame = true;
		}

		void Update(double rawDeltaTime)
		{
			if (m_timer <= 0.0)
			{
				m_timeScale = 1.0;
				m_requestedThisFrame = false;
				return;
			}

			// リクエストされた同じフレームで減算すると、実際に止まる前に効果時間が短くなる。
			// 次フレームから rawDeltaTime で減らすことで、最初の停止フレームを確実に残す。
			if (m_requestedThisFrame)
			{
				m_requestedThisFrame = false;
				return;
			}

			m_timer -= rawDeltaTime;
			if (m_timer <= 0.0)
			{
				m_timer = 0.0;
				m_timeScale = 1.0;
			}
		}

		double Apply(double rawDeltaTime) const
		{
			return rawDeltaTime * m_timeScale;
		}

		bool IsActive() const
		{
			return m_timer > 0.0;
		}
	};
}
