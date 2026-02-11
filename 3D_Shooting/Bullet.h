#pragma once
#include "stdafx.h"

namespace shooting {
	
	class DefaultBullet : public IBullet {
	private:
		float m_Speed;
		bool m_IsActive;
		//寿命
		double m_LifeTime;
		//経過時間
		double m_ElapsedTime;
	public:
		DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~DefaultBullet() {}

		bool IsActive() const noexcept;
		void SetActive(bool active) noexcept;

		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);

		void ResetLife() noexcept
		{
			m_ElapsedTime = 0.0;
			SetActive(true);
		}
	};
}