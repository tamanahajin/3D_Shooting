#include "stdafx.h"
#include "Project.h"
#include "PlayerAimResolver.h"
#include "PlayerShotEffects.h"

namespace shooting {

	namespace
	{
		const float kNormalShotRange = 60.0f;
		const int kNormalShotDamage = 1;
		const double kNormalShotCooldown = 0.12;
		const double kBombShotCooldown = 1.0;
		const Vec3 kBombProjectileScale(0.01f, 0.01f, 0.01f);

		void ApplyHitscanDamage(
			const std::shared_ptr<GameObject>& shooter,
			const RaycastHit& hit,
			int damage)
		{
			if (damage <= 0)
			{
				return;
			}

			auto target = hit.m_Object.lock();
			if (!target || target->FindTag(L"Player"))
			{
				return;
			}

			DamageInfo info;
			info.m_Damage = GameDebugSettingsStore::ApplyPlayerDamageMultiplier(damage);
			if (info.m_Damage <= 0)
			{
				return;
			}
			info.m_Instigator = shooter;
			info.m_HitPoint = hit.m_Point;
			info.m_HitNormal = hit.m_Normal;

			if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(target))
			{
				enemyProxy->ApplyDamage(info);
				return;
			}

			if (auto health = target->GetComponent<Health>(false))
			{
				health->ApplyDamage(info);
			}
		}
	}

	void Player::AddBombAmmo(int amount)
	{
		if (amount <= 0)
		{
			return;
		}

		m_bombAmmo += amount;
		m_currentBullet = BulletType::Bomb;
	}

	void Player::UpdateCombat(double elapsedTime, bool hitStopActive)
	{
		m_shotCool -= elapsedTime;

		if (m_currentBullet == BulletType::Bomb && m_bombAmmo <= 0)
		{
			m_currentBullet = BulletType::Default;
		}

		const auto& input = App::GetInputDevice();
		const bool fireInput = input.KeyDown(VK_LBUTTON) || input.KeyDown('J');
		const bool canFire = !hitStopActive && fireInput && m_shotCool <= 0.0;
		const bool bombMode = IsBombMode();
		auto collisionManager = m_collisionManager.lock();

		PlayerBombAim bombAim;
		if (bombMode && m_bombPreview && m_mainCamera && collisionManager)
		{
			bombAim = PlayerAimResolver::ResolveBombAim(
				GetThis<Player>(),
				m_mainCamera,
				collisionManager,
				m_bombPreview->GetMaxRange());
		}

		if (m_bombPreview)
		{
			m_bombPreview->SetPreviewInput(
				bombMode && bombAim.isValid,
				bombAim.start,
				bombAim.aimPoint,
				bombAim.hitNormal,
				bombAim.hasHit);
		}

		if (!canFire)
		{
			return;
		}

		if (bombMode)
		{
			if (bombAim.isValid)
			{
				FireBomb(bombAim);
			}
			return;
		}

		if (!m_mainCamera || !collisionManager)
		{
			return;
		}

		const PlayerNormalShotAim normalAim = PlayerAimResolver::ResolveNormalShot(
			GetThis<Player>(),
			m_mainCamera,
			collisionManager,
			kNormalShotRange);
		if (normalAim.isValid)
		{
			FireNormalShot(normalAim);
		}
	}

	void Player::FaceAttackTarget(const Vec3& target)
	{
		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		Vec3 attackDirection = target - transform->GetPosition();
		attackDirection.y = 0.0f;
		if (attackDirection.length() <= 1e-6f)
		{
			return;
		}

		attackDirection.normalize();
		if (auto util = GetBehavior<UtilBehavior>())
		{
			util->RotToHead(attackDirection, 1.0f);
		}
	}

	void Player::FireBomb(const PlayerBombAim& aim)
	{
		auto bulletManager =
			GetStage()->GetSharedGameObjectEx<BulletManager>(L"BulletManager", false);
		if (!bulletManager || !m_bombPreview)
		{
			return;
		}

		// プレビューと実弾で同じ値を使い、表示した軌道と実際の着弾位置がずれないようにする。
		const Vec3 target = aim.aimPoint;
		const Vec3 hitNormal = aim.hitNormal;
		const bool hasHit = aim.hasHit;
		const BombTuning tuning = m_bombPreview->GetTuning();
		bulletManager->FireEx<BombBullet>(
			aim.start,
			aim.rotation,
			kBombProjectileScale,
			[target, hitNormal, hasHit, tuning](BombBullet& bomb)
			{
				bomb.SetAimFromPreview(target, tuning, hitNormal, hasHit);
			});

		GameAudio::Instance().PlaySound(GameSoundId::BombThrow);
		m_shotCool = kBombShotCooldown;
		--m_bombAmmo;
		if (m_bombAmmo <= 0)
		{
			m_bombAmmo = 0;
			m_currentBullet = BulletType::Default;
		}

		FaceAttackTarget(aim.aimPoint);
		if (auto anim = GetBehavior<AnimationStateBehavior>())
		{
			anim->ChangeAnimation(AnimState::AttackMeleeLeft);
		}
	}

	void Player::FireNormalShot(const PlayerNormalShotAim& aim)
	{
		GameAudio::Instance().PlaySound(GameSoundId::PlayerShot);
		if (aim.hasHit)
		{
			ApplyHitscanDamage(GetThis<GameObject>(), aim.hit, kNormalShotDamage);
		}

		FaceAttackTarget(aim.aimPoint);
		m_shotCool = kNormalShotCooldown;

		Vec3 shotForward = aim.aimPoint - aim.muzzle;
		if (shotForward.length() > 1e-6f)
		{
			shotForward.normalize();
		}
		else if (auto transform = GetComponent<Transform>(false))
		{
			shotForward = transform->GetForward();
		}

		SpawnPlayerMuzzleFlash(GetThis<Player>(), aim.muzzle, shotForward);

		if (aim.hasHit)
		{
			SpawnPlayerBulletImpactSpark(
				GetStage(false),
				aim.muzzle,
				aim.hit,
				shotForward);
		}
		SpawnPlayerBulletTracer(
			GetStage(false),
			aim.muzzle,
			aim.aimPoint,
			shotForward);

		if (auto anim = GetBehavior<AnimationStateBehavior>())
		{
			anim->ChangeAnimation(AnimState::HoldingRightShoot, true);
		}
	}

}
