#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace {
		constexpr float kMinimumKnockbackUpwardSpeed = 10.0f;
		constexpr float kKnockbackInitialLift = 0.08f;
		constexpr double kKnockbackLaunchSeconds = 0.12;

		float RandomRange(float minValue, float maxValue)
		{
			return minValue + (maxValue - minValue) * Util::RandZeroToOne(true);
		}
	}

	void EnemyController::KillEnemy(EnemyState& enemy)
	{
		if (enemy.isDead)
		{
			return;
		}

		if (enemy.dropExperienceOnDeath && enemy.position.y >= kFallDeathY)
		{
			if (auto gameStage = m_gameStage.lock())
			{
				gameStage->SpawnExperienceOrb(enemy.position, enemy.status.experienceReward, enemy.rotation);
			}
		}
		enemy.dropExperienceOnDeath = false;

		enemy.hp = 0;
		enemy.isDead = true;
		enemy.deathAnimFinished = false;
		enemy.landingDeathState = LandingDeathState::None;
		enemy.delayedDeathMinTimer = 0.0;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.velocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackControlTimer = 0.0;
		enemy.knockbackLaunchTimer = 0.0;
		// 死亡アニメーションへ入る時は、爆風用の描画回転を残さない。
		enemy.knockbackSpinRotation.identity();
		enemy.knockbackSpinAxis = Vec3(0.0f, 1.0f, 0.0f);
		enemy.knockbackSpinSpeed = 0.0f;
		enemy.knockbackSpinTimer = 0.0;
		ChangeAnimation(enemy, AnimState::Dead, true);
	}

	void EnemyController::KillByFall(EnemyState& enemy)
	{
		if (enemy.position.y >= kFallDeathY)
		{
			return;
		}

		KillEnemy(enemy);
	}

	void EnemyController::ShowDamageNumber(size_t index, const DamageInfo& info)
	{
		if (info.m_Damage <= 0 || index >= m_enemies.size())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			Vec3 damagePosition = m_enemies[index].position;
			damagePosition.y += m_enemies[index].status.damageNumberOffsetY;
			gameStage->SpawnDamageNumber(damagePosition, info.m_Damage);
		}
	}

	void EnemyController::StartHitPush(EnemyState& enemy, const DamageInfo& info)
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

	bool EnemyController::ApplyDamage(size_t index, const DamageInfo& info)
	{
		if (index >= m_enemies.size())
		{
			return false;
		}

		auto& enemy = m_enemies[index];
		// 着地後の死亡が確定している敵は、別の攻撃で撃破数やダメージを重複計上しない。
		if (!enemy.active ||
			enemy.isDead ||
			enemy.landingDeathState != LandingDeathState::None ||
			info.m_Damage <= 0)
		{
			return false;
		}

		// 総ダメージには残りHPを超えたオーバーキル分を含めない。
		const int appliedDamage = std::min(enemy.hp, info.m_Damage);
		if (auto gameStage = m_gameStage.lock())
		{
			gameStage->RecordDamageDealt(appliedDamage);
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
				// 爆弾撃破後も敵本体は吹っ飛ばすため、経験値だけは移動前の位置に先に落とす。
				if (enemy.position.y >= kFallDeathY)
				{
					if (auto gameStage = m_gameStage.lock())
					{
						gameStage->SpawnExperienceOrb(enemy.position, enemy.status.experienceReward, enemy.rotation);
					}
				}
				// 爆弾の致死ダメージは即死亡にせず、吹っ飛んだ後の接地で死亡させる。
				enemy.hp = 1;
				enemy.landingDeathState = LandingDeathState::WaitingForAirborne;
				enemy.delayedDeathMinTimer = 0.12;
				return true;
			}

			enemy.dropExperienceOnDeath = true;
			KillEnemy(enemy);
			return true;
		}

		return false;
	}

	void EnemyController::AddKnockback(size_t index, const Vec3& velocity)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto& enemy = m_enemies[index];
		if (!enemy.active || enemy.isDead)
		{
			return;
		}

		enemy.knockbackVelocity = Vec3(velocity.x, 0.0f, velocity.z);
		// 爆心より下にいる場合も下向きや接地速度に負けないよう、上昇速度の最低値を保証する。
		enemy.gravityVelocity.y = bsmUtil::Max(
			enemy.gravityVelocity.y,
			bsmUtil::Max(velocity.y, kMinimumKnockbackUpwardSpeed));
		enemy.gravityVelocity.x = 0.0f;
		enemy.gravityVelocity.z = 0.0f;
		enemy.position.y += kKnockbackInitialLift;
		enemy.velocity *= 0.2f;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.isGround = false;
		enemy.knockbackControlTimer = 0.45;
		enemy.knockbackLaunchTimer = kKnockbackLaunchSeconds;
		if (enemy.landingDeathState == LandingDeathState::WaitingForAirborne)
		{
			enemy.landingDeathState = LandingDeathState::WaitingForLanding;
		}

		AddRandomRotation(index);
	}

	void EnemyController::AddRandomRotation(size_t index)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto& enemy = m_enemies[index];
		if (!enemy.active || enemy.isDead)
		{
			return;
		}

		Vec3 axis(
			RandomRange(-1.0f, 1.0f),
			RandomRange(0.2f, 1.0f),
			RandomRange(-1.0f, 1.0f));

		// 乱数がほぼゼロベクトルになると回転軸として使えないため、念のため上方向へ倒す。
		if (bsmUtil::lengthSqr(axis) <= 1e-6f)
		{
			axis = Vec3(0.0f, 1.0f, 0.0f);
		}
		axis.normalize();

		// 回転方向も敵ごとに変えると、同じ爆風でも全員が同じ姿勢になりにくい。
		const float sign = Util::RandZeroToOne(true) < 0.5f ? -1.0f : 1.0f;

		enemy.knockbackSpinAxis = axis;
		// 1.5から3.5回転/秒程度。
		enemy.knockbackSpinSpeed = sign * RandomRange(XM_2PI * 1.5f, XM_2PI * 3.5f);
		enemy.knockbackSpinTimer = 0.65;
		// 新しい爆風を受けた時は、前回の最終姿勢を引きずらず最初から回す。
		enemy.knockbackSpinRotation.identity();
	}

	void EnemyController::NotifyGroundCollision(size_t index, const CollisionPair& pair)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto& enemy = m_enemies[index];
		if (!enemy.active)
		{
			return;
		}

		if (enemy.knockbackLaunchTimer > 0.0 && enemy.gravityVelocity.y > 0.0f)
		{
			// 離陸直後の床接触は前フレームの重なりが残っているだけなので、接地へ戻さない。
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

