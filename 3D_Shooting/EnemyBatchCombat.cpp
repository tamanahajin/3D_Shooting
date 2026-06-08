/*!
@file EnemyBatchCombat.cpp
@brief 敵バッチのダメージ、死亡、ノックバック処理
HPや被弾演出の状態はEnemyStateに集約し、物理プロキシからはindex指定でここへ転送する。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	/*!
	@brief 敵を死亡状態へ切り替える
	@param enemy 対象敵の状態

	HP、移動力、ノックバック、遅延死亡フラグをリセットし、死亡アニメーションを先頭から再生する。
	*/
	void EnemyBatchController::KillEnemy(EnemyState& enemy)
	{
		if (enemy.isDead)
		{
			return;
		}

		enemy.hp = 0;
		enemy.isDead = true;
		enemy.deathAnimFinished = false;
		enemy.delayDeathUntilLanding = false;
		enemy.delayedDeathWasAirborne = false;
		enemy.delayedDeathMinTimer = 0.0;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.velocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackControlTimer = 0.0;
		ChangeAnimation(enemy, AnimState::Dead, true);
	}

	/*!
	@brief 落下死ラインを超えた敵を死亡させる
	@param enemy 対象敵の状態
	*/
	void EnemyBatchController::KillByFall(EnemyState& enemy)
	{
		if (enemy.position.y >= kFallDeathY)
		{
			return;
		}

		KillEnemy(enemy);
	}

	/*!
	@brief 敵の頭上付近にダメージ数値を表示する
	@param index 対象敵のインデックス
	@param info ダメージ情報
	*/
	void EnemyBatchController::ShowDamageNumber(size_t index, const DamageInfo& info)
	{
		if (info.m_Damage <= 0 || index >= m_Enemies.size())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			Vec3 damagePosition = m_Enemies[index].position;
			damagePosition.y += m_Enemies[index].status.damageNumberOffsetY;
			gameStage->SpawnDamageNumber(damagePosition, info.m_Damage);
		}
	}

	/*!
	@brief 通常被弾時の押され演出を開始する
	@param enemy 対象敵の状態
	@param info ダメージ情報

	実座標は動かさず、描画用の位置補正と傾きに使う方向だけを決める。
	*/
	void EnemyBatchController::StartHitPush(EnemyState& enemy, const DamageInfo& info)
	{
		enemy.hitPushDuration = enemy.status.hitPushDuration;
		enemy.hitPushDistance = enemy.status.hitPushDistance;
		enemy.hitPushLeanAngle = enemy.status.hitPushLeanAngle;
		enemy.hitPushTimer = enemy.hitPushDuration;

		Vec3 direction(0.0f, 0.0f, 0.0f);
		auto instigator = info.m_Instigator.lock();
		if (instigator)
		{
			auto instigatorTransform = instigator->GetComponent<Transform>(false);
			if (instigatorTransform)
			{
				direction = enemy.position - instigatorTransform->GetWorldPosition();
			}
		}

		// 通常弾は撃った側から敵が軽く押される見た目にしたいので、攻撃者から敵への水平向きを使う。
		direction.y = 0.0f;
		if (!bsmUtil::IsFiniteVec3(direction) || bsmUtil::lengthSqr(direction) <= 1e-6f)
		{
			direction = info.m_HitNormal;
			direction.y = 0.0f;
		}
		if (!bsmUtil::IsFiniteVec3(direction) || bsmUtil::lengthSqr(direction) <= 1e-6f)
		{
			direction = Vec3(1.0f, 0.0f, 0.0f);
		}

		direction.normalize();
		enemy.hitPushDirection = direction;
	}

	/*!
	@brief 指定敵へダメージを適用する
	@param index 対象敵のインデックス
	@param info ダメージ量、攻撃者、死亡遅延などの情報
	@return このダメージで即死亡した場合は true

	爆弾などで死亡遅延が指定された場合は、HPを一時的に1へ戻して着地後に死亡させる。
	*/
	bool EnemyBatchController::ApplyDamage(size_t index, const DamageInfo& info)
	{
		if (index >= m_Enemies.size())
		{
			return false;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active || enemy.isDead || info.m_Damage <= 0)
		{
			return false;
		}

		ShowDamageNumber(index, info);
		StartDamageFlash(enemy, enemy.status.damageFlashDuration);
		if (!info.m_DelayDeathUntilLanding)
		{
			StartHitPush(enemy, info);
		}

		enemy.hp -= bsmUtil::Clamp(info.m_Damage, 0, info.m_Damage);
		GameAudio::Instance().PlaySound(GameSoundId::EnemyDamage);

		if (enemy.hp <= 0)
		{
			if (info.m_DelayDeathUntilLanding)
			{
				// 爆弾の致死ダメージは即死亡にせず、吹っ飛んだ後の接地で死亡させる。
				enemy.hp = 1;
				enemy.delayDeathUntilLanding = true;
				enemy.delayedDeathWasAirborne = false;
				enemy.delayedDeathMinTimer = 0.12;
				return false;
			}

			KillEnemy(enemy);
			return true;
		}

		return false;
	}

	/*!
	@brief 指定敵へ爆風などのノックバック速度を与える
	@param index 対象敵のインデックス
	@param velocity 与える速度。水平成分と上方向成分を分けて扱う

	通常の追跡速度は弱め、一定時間はAI制御よりノックバックを優先する。
	*/
	void EnemyBatchController::AddKnockback(size_t index, const Vec3& velocity)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active || enemy.isDead)
		{
			return;
		}

		enemy.knockbackVelocity = Vec3(velocity.x, 0.0f, velocity.z);
		enemy.gravityVelocity.y = bsmUtil::Max(enemy.gravityVelocity.y, velocity.y);
		enemy.gravityVelocity.x = 0.0f;
		enemy.gravityVelocity.z = 0.0f;
		enemy.velocity *= 0.2f;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.isGround = false;
		enemy.knockbackControlTimer = 0.45;
	}

	/*!
	@brief プロキシ側で検出した接地衝突を敵状態へ反映する
	@param index 対象敵のインデックス
	@param pair 衝突情報

	CollisionManager 側の床押し戻し結果を、バッチ配列側の接地状態と重力速度へ戻す。
	*/
	void EnemyBatchController::NotifyGroundCollision(size_t index, const CollisionPair& pair)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active)
		{
			return;
		}

		bool isGrounded = enemy.isGround;
		Vec3 gravityVelocity = enemy.gravityVelocity;
		if (!TryApplyGroundCollision(pair, gravityVelocity, isGrounded))
		{
			return;
		}

		enemy.isGround = isGrounded;
		enemy.gravityVelocity = gravityVelocity;
		enemy.gravityVelocity.x = 0.0f;
		enemy.gravityVelocity.z = 0.0f;

		auto proxy = enemy.proxy.lock();
		if (proxy)
		{
			auto transform = proxy->GetComponent<Transform>(false);
			if (transform)
			{
				enemy.position = transform->GetPosition();
			}
		}
	}
}

