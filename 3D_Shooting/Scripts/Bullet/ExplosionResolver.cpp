/*!
@file ExplosionResolver.cpp
@brief ExplosionResolverの実装
*/

#include "stdafx.h"
#include "Project.h"
#include "ExplosionResolver.h"

namespace shooting {

	void ExplosionResolver::Reset() noexcept
	{
		m_hitTargets.clear();
		m_killCount = 0;
	}

	void ExplosionResolver::StartExplosion(
		const std::shared_ptr<GameObject>& source,
		const std::shared_ptr<GameObject>& firstHit)
	{
		if (!source) return;

		GameAudio::Instance().PlaySound(GameSoundId::BombExplode);
		m_hitTargets.clear();

		Vec3 explosionPosition(0.0f, 0.0f, 0.0f);
		if (auto transform = source->GetComponent<Transform>(false))
		{
			explosionPosition = transform->GetPosition();
			transform->SetScale(Vec3(m_explosionScale, m_explosionScale, m_explosionScale));
		}

		auto stage = source->GetStage(false);
		if (stage)
		{
			if (auto camera = std::dynamic_pointer_cast<MainCamera>(stage->GetCamera()))
			{
				const auto& tuning = GetBombTuning();
				camera->RequestCameraShake(
					explosionPosition,
					tuning.cameraShakeIntensity,
					tuning.cameraShakeDuration,
					tuning.cameraShakeMaxDistance);
			}

			TransParam effectParam;
			effectParam.position = explosionPosition;
			effectParam.scale = Vec3(1.0f, 1.0f, 1.0f);
			effectParam.quaternion = Quat();

			auto effect = stage->AddGameObject<ExplosionEffect>(effectParam);
			effect->SetLifeTime(0.2f);
			effect->SetScaleRange(0.15f, bsmUtil::Max(1.4f, m_explosionScale * 0.9f));
			effect->SetTextureKey(L"EXPLOSION_FIRE_TX");
		}

		// 爆風判定は残す必要があるため、GameObject自体は無効化せず見た目だけを隠す。
		source->SetDrawActive(false);
		source->SetShadowActive(false);

		if (firstHit)
		{
			ApplyToTarget(source, firstHit);
		}
	}

	void ExplosionResolver::ApplyToTarget(
		const std::shared_ptr<GameObject>& source,
		const std::shared_ptr<GameObject>& target)
	{
		if (!source || !target) return;

		if (!m_hitTargets.insert(target.get()).second)
		{
			return;
		}

		DamageInfo damageInfo;
		int damage = m_explosionDamage;
		if (auto damageDealer = source->GetComponent<DamageDealer>(false))
		{
			damage = damageDealer->GetDamage();
		}

		damageInfo.m_Damage = GameDebugSettingsStore::ApplyPlayerDamageMultiplier(damage);
		if (damageInfo.m_Damage <= 0)
		{
			return;
		}
		damageInfo.m_Instigator = source;
		damageInfo.m_DelayDeathUntilLanding = true;

		if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(target))
		{
			const bool defeated = enemyProxy->ApplyDamage(damageInfo);

			auto gameStage = std::dynamic_pointer_cast<GameStage>(source->GetStage(false));
			if (gameStage)
			{
				gameStage->RequestHitStop(0.1, 0.03);
				if (defeated)
				{
					// 撃破数は爆弾ごとに数え、BEST EXPLOSIONへ途中経過から反映する。
					++m_killCount;
					gameStage->RecordExplosionKills(m_killCount);
				}
			}

			if (!enemyProxy->IsAlive())
			{
				return;
			}

			auto sourceTransform = source->GetComponent<Transform>(false);
			auto targetTransform = target->GetComponent<Transform>(false);
			Vec3 knockbackVelocity(0.0f, 10.0f, 0.0f);
			if (sourceTransform && targetTransform)
			{
				Vec3 knockbackDirection =
					targetTransform->GetPosition() - sourceTransform->GetPosition();
				knockbackDirection.y = 0.0f;
				float distance = knockbackDirection.length();
				if (distance <= 0.01f)
				{
					knockbackDirection = Vec3(0.0f, 0.0f, 1.0f);
					distance = 0.0f;
				}

				knockbackDirection.normalize();
				const float maxKnockbackDistance = m_explosionScale * 0.5f;
				float strength =
					1.15f - bsmUtil::Min(distance / maxKnockbackDistance, 1.0f);
				strength = bsmUtil::Max(strength, 0.45f);

				knockbackVelocity = knockbackDirection * (10.0f * strength);
				knockbackVelocity.y = 18.0f * strength;
			}

			// 致死時も離陸待ち状態を完了できるよう、最低限の上向き速度を必ず渡す。
			enemyProxy->AddKnockback(knockbackVelocity);
			return;
		}

		damageInfo.m_DelayDeathUntilLanding = false;
		auto health = target->GetComponent<Health>(false);
		if (health)
		{
			health->ApplyDamage(damageInfo);
			if (health->IsDead())
			{
				return;
			}
		}

		auto gravity = target->GetComponent<Gravity>(false);
		auto sourceTransform = source->GetComponent<Transform>(false);
		auto targetTransform = target->GetComponent<Transform>(false);
		if (!gravity || !sourceTransform || !targetTransform)
		{
			return;
		}

		Vec3 knockbackDirection =
			targetTransform->GetPosition() - sourceTransform->GetPosition();
		knockbackDirection.y = 0.0f;
		const float distance = knockbackDirection.length();
		if (distance <= 0.01f)
		{
			return;
		}

		knockbackDirection.normalize();
		const float maxKnockbackDistance = m_explosionScale * 0.5f;
		float strength = 1.0f - bsmUtil::Min(distance / maxKnockbackDistance, 1.0f);
		strength = bsmUtil::Max(strength, 0.3f);

		Vec3 knockbackVelocity = knockbackDirection * (15.0f * strength);
		knockbackVelocity.y = 20.0f * strength;
		gravity->SetGravityVelocity(knockbackVelocity);
	}

}
