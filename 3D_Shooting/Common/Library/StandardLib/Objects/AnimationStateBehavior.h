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
			case AnimState::Idle: draw->SetAnimationIndex((int)state); break;
			case AnimState::Walk: draw->SetAnimationIndex((int)state); break;
			case AnimState::Sprint: draw->SetAnimationIndex((int)state); break;
			case AnimState::Jump: draw->SetAnimationIndex((int)state); break;
			case AnimState::Fall: draw->SetAnimationIndex((int)state); break;
			case AnimState::AttackMeleeLeft: draw->SetAnimationIndex((int)state); break;
			case AnimState::Dead: draw->SetAnimationIndex((int)state); break;
			}
		}

		void OnUpdate(double elapsedTime) override
		{
			m_Time += elapsedTime;

			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>();
			draw->UpdateAnimation(m_Time);
		}

		bool IsPlayingAttack() const
		{
			return m_Current == AnimState::AttackMeleeLeft
				|| m_Current == AnimState::AttackMeleeRight;
		}

		bool IsFinished() const
		{
			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>();
			return m_Time >= draw->GetCurrentAnimationDurationSeconds();
		}
	};
}