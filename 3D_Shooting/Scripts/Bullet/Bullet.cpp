#include "stdafx.h"
#include "Project.h"
#include <iostream>

namespace shooting {

	namespace
	{
		const wchar_t* kBombModelKey = L"BOMB_MODEL";
		const wchar_t* kBombMaterialPrefix = L"BOMB_MAT_";
	}

	//============================================================
	// DefaultBullet
	//============================================================
	DefaultBullet::DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: IBullet(stagePtr)
	{
		// ObjectFactory::Create のコンストラクタから OnCreate() を呼ぶので
		// Transformの初期値を先に設定したい場合はここで保管する
		m_transParam = param;
	}

	void DefaultBullet::OnCreate()
	{
		const auto& tuning = GetWeaponTuning();
		m_speed = tuning.defaultBulletSpeed;
		m_lifeTime = tuning.defaultBulletLifeTime;

		// 衝突
		auto ptrColl = AddComponent<CollisionSphere>();
		ptrColl->SetFixed(false);

		// タグ
		AddTag(L"Bullet");

		// 描画
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_SPHERE");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		ptrDraw->SetOwnShadowActive(true);

		// ダメージ（このプロジェクトでは OnCollisionEnter から ApplyDamage を呼ぶ）
		auto dmg = AddComponent<DamageDealer>();
		dmg->SetDamage(tuning.defaultBulletDamage);
		dmg->SetDestroyOnHit(true);

		// 無視オブジェクト
		if (auto col = GetComponent<Collision>(false))
		{
			col->AddExcludeCollisionTag(L"Player");
			col->AddExcludeCollisionTag(L"Bullet");
		}
	}

	void DefaultBullet::OnUpdate(double elapsedTime)
	{
		if (!IsActive()) return;

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		// 寿命
		m_elapsedTime += elapsedTime;
		if (m_elapsedTime >= m_lifeTime)
		{
			SetActive(false);
			return;
		}

		// 前進
		if (auto ptrTrans = GetComponent<Transform>())
		{
			auto& param = ptrTrans->GetTransParam();
			Vec3 forward = ptrTrans->GetForward();
			param.position += forward * m_speed * static_cast<float>(elapsedTime);
		}
	}

	void DefaultBullet::OnCollisionEnter(const CollisionPair& pair)
	{
		if (!IsActive()) return;

		auto other = pair.m_Dest.lock();
		if (!other) return;

		auto otherObj = other->GetGameObject();
		if (!otherObj) return;

		// プレイヤーは除外
		if (otherObj->FindTag(L"Player")) return;

		DamageInfo info;
		info.m_Damage = GameDebugSettingsStore::ApplyPlayerDamageMultiplier(GetComponent<DamageDealer>()->GetDamage());
		if (info.m_Damage <= 0)
		{
			SetActive(false);
			return;
		}
		info.m_Instigator = GetThis<GameObject>();

		if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(otherObj))
		{
			enemyProxy->ApplyDamage(info);
		}
		else if (auto hp = otherObj->GetComponent<Health>(false))
		{
			hp->ApplyDamage(info);
		}

		// ここで終了（プールから再利用は BulletPool 側）
		SetActive(false);
	}

	void DefaultBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 必要なら Enter と同様に処理
		// OnCollisionEnter(pair);
	}




	//============================================================
	// BombBullet
	//============================================================
	BombBullet::BombBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: IBullet(stagePtr)
	{
		// OnCreateより先にTransformの初期値を渡す必要があるため、生成時の値を保持する。
		m_transParam = param;
	}

	void BombBullet::OnCreate()
	{
		const auto& tuning = GetWeaponTuning();
		m_speed = tuning.bombSpeed;
		m_defaultFuseTime = tuning.bombFuseTime;
		m_explosionDuration = tuning.explosionDuration;
		m_explosionResolver.SetExplosionDamage(tuning.explosionDamage);
		m_explosionResolver.SetExplosionScale(tuning.explosionRadius * 2.0f);

		auto ptrColl = AddComponent<CollisionSphere>();
		ptrColl->SetFixed(false);

		AddTag(L"Bullet");
		AddTag(L"Bomb");

		const auto& meshes = BaseScene::Get()->GetModelMesh(kBombModelKey);

		auto ptrShadow = AddComponent<ShadowMap>();
		if (!meshes.empty())
		{
			ptrShadow->AddBaseMesh(meshes[0]);
		}
		else
		{
			ptrShadow->AddBaseMesh(L"DEFAULT_SPHERE");
		}

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->SetFogEnabled(true);
		ptrDraw->SetOwnShadowActive(true);
		ptrDraw->SetLightingEnabled(true);

		if (!meshes.empty())
		{
			ptrDraw->AddBaseModelMesh(meshes);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				ptrDraw->AddBaseMaterial(std::wstring(kBombMaterialPrefix) + std::to_wstring(i));
			}
		}
		else
		{
			ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
			ptrDraw->AddBaseTexture(L"WALL_TX");
		}

		auto dmg = AddComponent<DamageDealer>();
		dmg->SetDamage(tuning.explosionDamage);
		dmg->SetDestroyOnHit(false);

		if (auto col = GetComponent<Collision>(false))
		{
			col->AddExcludeCollisionTag(L"Player");
			col->AddExcludeCollisionTag(L"Bullet");
		}
	}

	void BombBullet::OnUpdate(double elapsedTime)
	{
		if (!IsActive()) return;

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		if (m_state == BombState::Flying)
		{
			// 飛行中（信管）
			m_fuseTime -= elapsedTime;
			if (m_fuseTime <= 0.0)
			{
				StartExplosion(nullptr);
				return;
			}

			auto trans = GetComponent<Transform>();
			if (!trans) return;

			auto& tp = trans->GetTransParam();
			const Vec3 previousPosition = tp.position;

			if (m_useBallistic)
			{
				// 発射からの経過 t
				m_flyTime += static_cast<float>(elapsedTime);

				const float t = m_flyTime;
				tp.position = BallisticTrajectory::SamplePosition(
					m_startPos,
					m_v0,
					m_gravity,
					t);
				m_velocity = BallisticTrajectory::SampleVelocity(m_v0, m_gravity, t);

				Vec3 impactPosition;
				if (m_useGeneratedGroundImpact &&
					BombImpactResolver::TryResolveGeneratedGroundImpact(
						GetStage(false),
						previousPosition,
						tp.position,
						impactPosition))
				{
					tp.position = impactPosition;
					StartExplosion(nullptr);
					return;
				}

				// --- 重要： 「プレビュー終点」 と一致させるコツ ---
				// プレビューは 0..T で描画して終了なので、弾側も T で終了または着弾させる
				if (m_flyTime >= m_totalT)
				{
					tp.position = m_targetPos;   // 最後ぴったりと合わせる
					StartExplosion(nullptr);     // 着弾で爆発（不要ならコメントアウトして停止など）
					return;
				}
			}
			else
			{
				// ターゲットなし：従来の直進（+重力）でもOK
				const float dt = static_cast<float>(elapsedTime);
				m_velocity += m_gravity * dt;
				tp.position += m_velocity * dt;

				Vec3 directImpactPosition;
				if (m_useGeneratedGroundImpact &&
					BombImpactResolver::TryResolveGeneratedGroundImpact(
						GetStage(false),
						previousPosition,
						tp.position,
						directImpactPosition))
				{
					tp.position = directImpactPosition;
					StartExplosion(nullptr);
					return;
				}
			}
		}
		else
		{
			m_explosionTimer -= elapsedTime;
			if (m_explosionTimer <= 0.0)
			{
				SetActive(false);
				return;
			}
		}
	}
	void BombBullet::OnCollisionEnter(const CollisionPair& pair)
	{
		if (!IsActive()) return;

		auto other = pair.m_Dest.lock();
		if (!other) return;

		auto otherObj = other->GetGameObject();
		if (!otherObj) return;

		if (otherObj->FindTag(L"Player")) return;

		if (m_state == BombState::Flying)
		{
			// 飛行中に何かに当たったら即爆発
			StartExplosion(otherObj);
			return;
		}

		// 爆発中は範囲ダメージ（多重ヒット防止）
		m_explosionResolver.ApplyToTarget(GetThis<GameObject>(), otherObj);
	}

	void BombBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 爆発セット後場合 Enter だけでは不十分なときある。
		// 連続衝突イベントの仕様次第で Execute の方が確実ならここで処理する。
		if (m_state == BombState::Exploding)
		{
			OnCollisionEnter(pair);
		}
	}

	void BombBullet::StartExplosion(const std::shared_ptr<GameObject>& firstHit)
	{
		if (m_state == BombState::Exploding) return;

		m_state = BombState::Exploding;
		m_explosionTimer = m_explosionDuration;
		m_velocity = Vec3(0, 0, 0);
		m_explosionResolver.StartExplosion(GetThis<GameObject>(), firstHit);
	}

	void BombBullet::ResetForSpawn() noexcept
	{
		const auto& tuning = GetWeaponTuning();
		m_speed = tuning.bombSpeed;
		m_defaultFuseTime = tuning.bombFuseTime;
		m_explosionDuration = tuning.explosionDuration;
		m_arcHeight = tuning.arcHeightBase;
		m_gravity = tuning.gravity;
		m_arcHeightPerDistXZ = tuning.arcHeightPerDistXZ;
		m_explosionResolver.SetExplosionDamage(tuning.explosionDamage);
		m_explosionResolver.SetExplosionScale(tuning.explosionRadius * 2.0f);
		m_fuseTime = m_defaultFuseTime;
		m_state = BombState::Flying;
		m_explosionTimer = 0.0;
		m_explosionResolver.Reset();

		m_flyTime = 0.0f;
		m_totalT = 0.0f;
		m_useBallistic = false;
		m_useGeneratedGroundImpact = true;

		auto trans = GetComponent<Transform>(false);
		if (!trans)
		{
			m_hasTarget = false;
			m_hasTargetSurface = false;
			return;
		}

		// 発射時の起点 p0
		m_startPos = trans->GetTransParam().position;

		// ターゲットなければ信管で直進
		if (!m_hasTarget)
		{
			m_v0 = trans->GetForward() * m_speed;
			m_velocity = m_v0;
			m_impactIgnoredObject.reset();
			return;
		}

		auto ignoredObject = m_impactIgnoredObject.lock();
		if (!ignoredObject)
		{
			ignoredObject = GetThis<GameObject>();
		}
		const BombImpactSurface surface = BombImpactResolver::ResolveTargetSurface(
			GetStage(false),
			ignoredObject,
			m_startPos,
			m_targetPos,
			m_targetNormal,
			m_hasTargetSurface);
		m_targetPos = surface.position;
		m_targetNormal = surface.normal;
		m_hasTargetSurface = surface.hasSurface;
		m_useGeneratedGroundImpact =
			BombImpactResolver::ShouldCheckGeneratedGroundImpact(m_hasTargetSurface, m_targetNormal);

		const float arcHeight = BallisticTrajectory::CalculateArcHeight(
			m_startPos,
			m_targetPos,
			m_arcHeight,
			m_arcHeightPerDistXZ);

		BallisticTrajectorySolution trajectory;
		if (BallisticTrajectory::TrySolveApexHeight(
			m_startPos,
			m_targetPos,
			m_gravity,
			arcHeight,
			trajectory))
		{
			m_v0 = trajectory.initialVelocity;
			m_totalT = trajectory.duration;
			m_useBallistic = true;

			// 初速（向き回転などに使うなら）
			m_velocity = m_v0;

			// ターゲットに到達する前に信管が切れないよう信管
			// （着弾に爆発させないなら、この信管は事実上不要）
			m_fuseTime = bsmUtil::Max(
				m_fuseTime,
				static_cast<double>(trajectory.duration) + 0.2);
		}
		else
		{
			// 解けなかったら従来の直進にフォールバック
			m_v0 = trans->GetForward() * m_speed;
			m_velocity = m_v0;
			m_useBallistic = false;
		}

		// 繰り返し時の防止
		m_hasTarget = false;
		m_hasTargetSurface = false;
		m_impactIgnoredObject.reset();
	}
}
