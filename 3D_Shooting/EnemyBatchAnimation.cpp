/*!
@file EnemyBatchAnimation.cpp
@brief 謨ｵ繝舌ャ繝√・繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ譎る俣邂｡逅・
豁ｻ莠｡繧・判謦・↑縺ｩ縺ｮ蜊倡匱繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｯ譛蠕後・1繝輔Ξ繝ｼ繝謇句燕縺ｧ豁｢繧√ｋ縲・繝ｫ繝ｼ繝礼ｳｻ繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｯ騾壼ｸｸ騾壹ｊ譎る俣繧帝ｲ繧√ｋ縲・*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	bool EnemyBatchController::IsOneShotState(AnimState state) const
	{
		switch (state)
		{
		case AnimState::Dead:
		case AnimState::AttackMeleeLeft:
		case AnimState::AttackMeleeRight:
			return true;
		default:
			return false;
		}
	}

	bool EnemyBatchController::IsHoldLastFrameState(AnimState state) const
	{
		switch (state)
		{
		case AnimState::Dead:
		case AnimState::AttackMeleeLeft:
		case AnimState::AttackMeleeRight:
			return true;
		default:
			return false;
		}
	}

	double EnemyBatchController::GetAnimationDurationSeconds(AnimState state) const
	{
		auto mesh = BaseScene::Get()->GetMesh(L"ENEMY_MODEL_SKINNED");
		if (!mesh)
		{
			return 0.0;
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return 0.0;
		}

		const unsigned int index = static_cast<unsigned int>(state);
		if (index >= static_cast<unsigned int>(assimp->GetAnimationCount()))
		{
			return 0.0;
		}

		return static_cast<double>(assimp->GetAnimationDurationSeconds(index));
	}

	double EnemyBatchController::GetHoldTimeSeconds(double duration) const
	{
		return bsmUtil::Max(0.0, duration - (1.0 / 30.0));
	}

	void EnemyBatchController::ChangeAnimation(EnemyState& enemy, AnimState state, bool forceRestart)
	{
		if (!forceRestart && enemy.animationState == state)
		{
			return;
		}

		enemy.animationState = state;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
	}

	void EnemyBatchController::UpdateAnimation(EnemyState& enemy, double elapsedTime)
	{
		if (IsOneShotState(enemy.animationState))
		{
			double duration = GetAnimationDurationSeconds(enemy.animationState);
			if (duration <= 0.0)
			{
				duration = 0.6;
			}

			if (!enemy.animationFinished)
			{
				enemy.animationTime += elapsedTime;
				if (enemy.animationTime >= duration)
				{
					enemy.animationFinished = true;
					enemy.animationTime = IsHoldLastFrameState(enemy.animationState)
						? GetHoldTimeSeconds(duration)
						: duration;
				}
			}
			else if (IsHoldLastFrameState(enemy.animationState))
			{
				enemy.animationTime = GetHoldTimeSeconds(duration);
			}

			return;
		}

		enemy.animationTime += elapsedTime;
	}

	float EnemyBatchController::GetDamageFlashValue(const EnemyState& enemy) const
	{
		if (enemy.damageFlashDuration <= 0.0 || enemy.damageFlashTimer <= 0.0)
		{
			return 0.0f;
		}

		double value = enemy.damageFlashTimer / enemy.damageFlashDuration;
		if (value < 0.0)
		{
			value = 0.0;
		}
		else if (value > 1.0)
		{
			value = 1.0;
		}
		return static_cast<float>(value);
	}

	void EnemyBatchController::StartDamageFlash(EnemyState& enemy, double duration)
	{
		if (duration <= 0.0)
		{
			duration = 0.001;
		}
		enemy.damageFlashDuration = duration;
		enemy.damageFlashTimer = duration;
	}
}

