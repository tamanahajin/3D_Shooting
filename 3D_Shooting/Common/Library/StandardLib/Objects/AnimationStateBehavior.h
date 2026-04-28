#pragma once
#include "stdafx.h"

namespace shooting {

	class AnimationStateBehavior : public Behavior
	{
	private:
		AnimState m_Current = AnimState::Idle;
		double m_Time = 0.0;
		bool m_Finished = false;

		bool IsOneShotState(AnimState state) const
		{
			switch (state)
			{
			case AnimState::Dead:
			case AnimState::AttackMeleeLeft:
			case AnimState::AttackMeleeRight:
			//case AnimState::Damage:
				return true;
			default:
				return false;
			}
		}

		bool IsHoldLastFrameState(AnimState state) const
		{
			switch (state)
			{
			case AnimState::Dead:
			case AnimState::AttackMeleeLeft:
			case AnimState::AttackMeleeRight:
			//case AnimState::Damage:
				return true;
			default:
				return false;
			}
		}

	public:
		AnimationStateBehavior(const std::shared_ptr<GameObject>& obj)
			: Behavior(obj)
		{
		}

		void ChangeAnimation(AnimState state, bool forceRestart = false)
		{
			if (!forceRestart && m_Current == state)
			{
				return;
			}

			m_Current = state;
			m_Time = 0.0;
			m_Finished = false;

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

			// 切替直後の姿勢を1回反映
			draw->UpdateAnimation(0.0);
		}

		void OnUpdate(double elapsedTime) override
		{
			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>();
			if (!draw)
			{
				return;
			}

			const double duration = static_cast<double>(draw->GetCurrentAnimationDurationSeconds());

			// 長さが取れない場合は従来通り
			if (duration <= 0.0)
			{
				m_Time += elapsedTime;
				draw->UpdateAnimation(m_Time);
				return;
			}

			if (IsOneShotState(m_Current))
			{
				if (!m_Finished)
				{
					m_Time += elapsedTime;

					if (m_Time >= duration)
					{
						m_Finished = true;

						if (IsHoldLastFrameState(m_Current))
						{
							// 最終フレーム直前で固定
							const double holdTime = bsmUtil::Max(0.0, duration - (1.0 / 60.0));
							m_Time = holdTime;
						}
						else
						{
							m_Time = duration;
						}
					}
				}

				draw->UpdateAnimation(m_Time);
				return;
			}

			// ループする通常アニメ
			m_Time += elapsedTime;
			draw->UpdateAnimation(m_Time);
		}


		bool IsPlayingAttack() const
		{
			return m_Current == AnimState::AttackMeleeLeft
				|| m_Current == AnimState::AttackMeleeRight;
		}

		bool IsFinished() const
		{
			return m_Finished;
		}

		bool IsPlayingOneShot() const
		{
			return IsOneShotState(m_Current) && !m_Finished;
		}

		AnimState GetCurrentState() const
		{
			return m_Current;
		}
	};
}