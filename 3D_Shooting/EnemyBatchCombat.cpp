/*!
@file EnemyBatchCombat.cpp
@brief 敵バッチのダメージ、死亡、ノックバック処理
HPや被弾演出の状態はEnemyStateに集約し、物理プロキシからはindex指定でここへ転送する。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

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

	void EnemyBatchController::KillByFall(EnemyState& enemy)
	{
		if (enemy.position.y >= kFallDeathY)
		{
			return;
		}

		KillEnemy(enemy);
	}

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

