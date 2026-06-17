/*!
@file EnemyAnimation.cpp
@brief 敵バッチのアニメーション時間管理
死亡や攻撃などの単発アニメーションは最後の1フレーム手前で止める。
ループ系アニメーションは通常通り時間を進める。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	/*!
	@brief 指定アニメーションが単発再生かを判定する
	@param state 対象アニメーション
	@return 単発再生として扱う場合は true
	*/
	bool EnemyController::IsOneShotState(AnimState state) const
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

	/*!
	@brief 指定アニメーションを最終フレーム付近で保持するかを判定する
	@param state 対象アニメーション
	@return 最終フレーム付近で停止させる場合は true
	*/
	bool EnemyController::IsHoldLastFrameState(AnimState state) const
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

	/*!
	@brief 敵モデルから指定アニメーションの再生時間を取得する
	@param state 対象アニメーション
	@return 秒単位の再生時間。取得できない場合は0
	*/
	double EnemyController::GetAnimationDurationSeconds(AnimState state) const
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

	/*!
	@brief 最終フレーム手前で止めるための停止時刻を返す
	@param duration アニメーション全体の長さ
	@return 停止に使う秒数
	*/
	double EnemyController::GetHoldTimeSeconds(double duration) const
	{
		return bsmUtil::Max(0.0, duration - (1.0 / 30.0));
	}

	/*!
	@brief 敵のアニメーション状態を変更する
	@param enemy 対象敵の状態
	@param state 変更先のアニメーション
	@param forceRestart 同じ状態でも先頭から再生する場合は true
	*/
	void EnemyController::ChangeAnimation(EnemyState& enemy, AnimState state, bool forceRestart)
	{
		if (!forceRestart && enemy.animationState == state)
		{
			return;
		}

		enemy.animationState = state;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
	}

	/*!
	@brief 敵のアニメーション時間を進める
	@param enemy 対象敵の状態
	@param elapsedTime 経過時間

	単発アニメーションは終了後に止め、ループ系は時間を進め続ける。
	*/
	void EnemyController::UpdateAnimation(EnemyState& enemy, double elapsedTime)
	{
		if (IsOneShotState(enemy.animationState))
		{
			double duration = GetAnimationDurationSeconds(enemy.animationState);
			if (duration <= 0.0)
			{
				duration = 0.6;
			}

			const bool holdLastFrame = IsHoldLastFrameState(enemy.animationState);
			const double stopTime = holdLastFrame ? GetHoldTimeSeconds(duration) : duration;

			if (!enemy.animationFinished)
			{
				enemy.animationTime += elapsedTime;

				if (enemy.animationTime >= stopTime)
				{
					enemy.animationFinished = true;
					enemy.animationTime = stopTime;
				}
			}
			else if (holdLastFrame)
			{
				enemy.animationTime = stopTime;
			}

			return;
		}

		enemy.animationTime += elapsedTime;
	}

	/*!
	@brief 被弾フラッシュの現在強度を取得する
	@param enemy 対象敵の状態
	@return 0.0から1.0のフラッシュ強度
	*/
	float EnemyController::GetDamageFlashValue(const EnemyState& enemy) const
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

	/*!
	@brief 被弾フラッシュタイマーを開始する
	@param enemy 対象敵の状態
	@param duration フラッシュ時間
	*/
	void EnemyController::StartDamageFlash(EnemyState& enemy, double duration)
	{
		if (duration <= 0.0)
		{
			duration = 0.001;
		}
		enemy.damageFlashDuration = duration;
		enemy.damageFlashTimer = duration;
	}
}

