#pragma once
#include "stdafx.h"

namespace shooting {

	class AnimationStateBehavior : public Behavior
	{
	private:
		AnimState m_Current = AnimState::Idle;
		double m_Time = 0.0;

	public:
		AnimationStateBehavior(const std::shared_ptr<GameObject>& obj)
			: Behavior(obj)
		{
		}

		void ChangeAnimation(AnimState state)
		{
			if (m_Current == state) return;

			m_Current = state;
			m_Time = 0.0;

			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>();

			switch (state)
			{
			case AnimState::Idle: draw->SetAnimationIndex(16); break;
			case AnimState::Walk: draw->SetAnimationIndex(24); break;
			case AnimState::Sprint: draw->SetAnimationIndex(22); break;
			case AnimState::Jump: draw->SetAnimationIndex(19); break;
			case AnimState::Fall: draw->SetAnimationIndex(9); break;
			}
		}

		void OnUpdate(double elapsedTime) override
		{
			m_Time += elapsedTime;

			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>();
			draw->UpdateAnimation(m_Time);
		}
	};
}