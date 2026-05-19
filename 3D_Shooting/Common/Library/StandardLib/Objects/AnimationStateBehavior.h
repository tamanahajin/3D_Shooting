#pragma once
#include "stdafx.h"

namespace shooting {

	class AnimationStateBehavior : public Behavior
	{
	private:
		AnimState m_Current = AnimState::Idle;
		double m_Time = 0.0;
		bool m_Finished = false;
		std::wstring m_FallbackMeshKey;

		bool IsOneShotState(AnimState state) const
		{
			switch (state)
			{
			case AnimState::Dead:
			case AnimState::AttackMeleeLeft:
			case AnimState::AttackMeleeRight:
			case AnimState::HoldingRightShoot:
			//case AnimState::Damage:
				return true;
			default:
				return false;
			}
		}

		double GetFallbackAnimationDurationSeconds() const
		{
			if (m_FallbackMeshKey.empty())
			{
				return 0.0;
			}

			auto mesh = BaseScene::Get()->GetMesh(m_FallbackMeshKey);
			if (!mesh)
			{
				return 0.0;
			}

			auto assimp = mesh->GetBaseAssimp();
			if (!assimp)
			{
				return 0.0;
			}

			const unsigned int index = static_cast<unsigned int>(m_Current);
			if (index >= static_cast<unsigned int>(assimp->GetAnimationCount()))
			{
				return 0.0;
			}

			return static_cast<double>(assimp->GetAnimationDurationSeconds(index));
		}
		double GetHoldTimeSeconds(double duration) const
		{
			return bsmUtil::Max(0.0, duration - (1.0 / 30.0));
		}
		double GetLoopEndTrimSeconds(AnimState state) const
		{
			switch (state)
			{
			case AnimState::Idle:
				return 2.0 / 30.0;
			default:
				return 0.0;
			}
		}
		double GetLoopSampleDurationSeconds(double duration) const
		{
			return bsmUtil::Max(0.0, duration - GetLoopEndTrimSeconds(m_Current));
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

			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>(false);
			if (!draw)
			{
				return;
			}

			draw->SetAnimationIndex(static_cast<unsigned int>(state));

			// 切替直後の姿勢を1回反映
			draw->UpdateAnimation(0.0);
		}

		void OnUpdate(double elapsedTime) override
		{
			auto draw = GetGameObject()->GetComponent<BcPNTBoneDraw>(false);
			if (!draw)
			{
				if (IsOneShotState(m_Current))
				{
					double duration = GetFallbackAnimationDurationSeconds();
					if (duration <= 0.0)
					{
						duration = 0.6;
					}

					if (!m_Finished)
					{
						m_Time += elapsedTime;

						if (m_Time >= duration)
						{
							m_Finished = true;

							if (IsHoldLastFrameState(m_Current))
							{
								m_Time = GetHoldTimeSeconds(duration);
							}
							else
							{
								m_Time = duration;
							}
						}
					}
					else if (IsHoldLastFrameState(m_Current))
					{
						m_Time = GetHoldTimeSeconds(duration);
					}

					return;
				}

				m_Time += elapsedTime;
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
							const double holdTime = GetHoldTimeSeconds(duration);
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

			// Looping animations can contain a bad duplicate/end frame exported from DCC tools.
			m_Time += elapsedTime;
			const double sampleDuration = GetLoopSampleDurationSeconds(duration);
			if (sampleDuration > 0.0)
			{
				m_Time = fmod(m_Time, sampleDuration);
				if (m_Time < 0.0)
				{
					m_Time += sampleDuration;
				}
			}
			draw->UpdateAnimation(m_Time);
		}


		void SetFallbackMeshKey(const std::wstring& key)
		{
			m_FallbackMeshKey = key;
		}

		bool IsPlayingAttack() const
		{
			return m_Current == AnimState::AttackMeleeLeft
				|| m_Current == AnimState::AttackMeleeRight
				|| m_Current == AnimState::HoldingRightShoot;
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

		double GetPlaybackTimeSeconds() const
		{
			if (IsOneShotState(m_Current) && IsHoldLastFrameState(m_Current))
			{
				double duration = GetFallbackAnimationDurationSeconds();
				if (duration > 0.0)
				{
					return bsmUtil::Min(m_Time, GetHoldTimeSeconds(duration));
				}
			}

			return m_Time;
		}
	};
}
