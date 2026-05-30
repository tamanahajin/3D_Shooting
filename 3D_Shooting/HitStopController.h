// HitStopController.h
#pragma once

namespace shooting {

	class HitStopController
	{
	private:
		double m_Timer = 0.0;
		double m_TimeScale = 1.0;
		bool m_RequestedThisFrame = false;

	public:
		void Request(double duration, double timeScale)
		{
			if (duration <= 0.0)
			{
				return;
			}

			timeScale = bsmUtil::Clamp(timeScale, 0.0, 1.0);
			m_Timer = bsmUtil::Max(m_Timer, duration);
			m_TimeScale = bsmUtil::Min(m_TimeScale, timeScale);
			m_RequestedThisFrame = true;
		}

		void Update(double rawDeltaTime)
		{
			if (m_Timer <= 0.0)
			{
				m_TimeScale = 1.0;
				m_RequestedThisFrame = false;
				return;
			}

			// リクエストされた同じフレームで減算すると、実際に止まる前に効果時間が短くなる。
			// 次フレームから rawDeltaTime で減らすことで、最初の停止フレームを確実に残す。
			if (m_RequestedThisFrame)
			{
				m_RequestedThisFrame = false;
				return;
			}

			m_Timer -= rawDeltaTime;
			if (m_Timer <= 0.0)
			{
				m_Timer = 0.0;
				m_TimeScale = 1.0;
			}
		}

		double Apply(double rawDeltaTime) const
		{
			return rawDeltaTime * m_TimeScale;
		}

		bool IsActive() const
		{
			return m_Timer > 0.0;
		}
	};
}
