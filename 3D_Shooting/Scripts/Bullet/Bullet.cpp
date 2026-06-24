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

		m_isActive = false;
	}

	void DefaultBullet::OnCreate()
	{
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
		dmg->SetDamage(3);
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
		if (!m_isActive) return;

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		// 寿命
		m_elapsedTime += elapsedTime;
		if (m_elapsedTime >= m_lifeTime)
		{
			SetActive(false);
			SetUpdateActive(false);
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

	bool DefaultBullet::IsActive() const noexcept
	{
		return m_isActive;
	}

	void DefaultBullet::SetActive(bool active) noexcept
	{
		m_isActive = active;
	}

	void DefaultBullet::OnCollisionEnter(const CollisionPair& pair)
	{
		if (!m_isActive) return;

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
		: DefaultBullet(stagePtr, param)
	{
	}

	void BombBullet::OnCreate()
	{
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
		dmg->SetDamage(m_explosionDamage);
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

		if (!m_exploding)
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

				// 弾道式：p(t)=p0+v0*t+0.5*g*t^2
				const float t = m_flyTime;
				tp.position = m_startPos + (m_v0 * t) + (m_gravity * (0.5f * t * t));

				// 現在速度：v(t)=v0+g*t（必要なら）
				m_velocity = m_v0 + (m_gravity * t);

				Vec3 impactPosition;
				if (m_useGeneratedGroundImpact && TryResolveTerrainImpact(previousPosition, tp.position, impactPosition))
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
				if (m_useGeneratedGroundImpact && TryResolveTerrainImpact(previousPosition, tp.position, directImpactPosition))
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
				SetUpdateActive(false);
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

		if (!m_exploding)
		{
			// 飛行中に何かに当たったら即爆発
			StartExplosion(otherObj);
			return;
		}

		// 爆発中は範囲ダメージ（多重ヒット防止）
		TryApplyExplosionDamage(otherObj);
	}

	void BombBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 爆発セット後場合 Enter だけでは不十分なときある。
		// 連続衝突イベントの仕様次第で Execute の方が確実ならここで処理する。
		if (m_exploding)
		{
			OnCollisionEnter(pair);
		}
	}

	void BombBullet::StartExplosion(const std::shared_ptr<GameObject>& firstHit)
	{
		if (m_exploding) return;

		GameAudio::Instance().PlaySound(GameSoundId::BombExplode);

		m_exploding = true;
		m_explosionTimer = m_explosionDuration;
		m_velocity = Vec3(0, 0, 0);
		m_hitOnce.clear();

		Vec3 explosionPos(0.0f, 0.0f, 0.0f);

		// 爆風範囲を「スケール拡大」で表現
		if (auto trans = GetComponent<Transform>())
		{
			explosionPos = trans->GetPosition();
			trans->SetScale(Vec3(m_explosionScale, m_explosionScale, m_explosionScale));
		}

		if (auto camera = std::dynamic_pointer_cast<MainCamera>(GetStage()->GetCamera()))
		{
			const auto& tuning = GetBombTuning();
			camera->RequestCameraShake(
				explosionPos,
				tuning.cameraShakeIntensity,
				tuning.cameraShakeDuration,
				tuning.cameraShakeMaxDistance);
		}

		// ゲームオブジェクト
		SetDrawActive(false);
		SetShadowActive(false);

		// 爆発VFX生成
		{
			TransParam fxParam;
			fxParam.position = explosionPos;
			fxParam.scale = Vec3(1.0f, 1.0f, 1.0f);
			fxParam.quaternion = Quat();

			auto fx = GetStage()->AddGameObject<ExplosionEffect>(fxParam);
			fx->SetLifeTime(0.2f);
			fx->SetScaleRange(0.15f, bsmUtil::Max(1.4f, m_explosionScale * 0.9f));
			fx->SetTextureKey(L"EXPLOSION_FIRE_TX");
		}

		// 弾側が誰かなら即ダメージ（爆発開始時点で衝突中の可能性があるため）
		if (firstHit)
		{
			TryApplyExplosionDamage(firstHit);
		}

	}

	void BombBullet::TryApplyExplosionDamage(const std::shared_ptr<GameObject>& target)
	{
		if (!target) return;

		// 多重ヒット防止
		if (m_hitOnce.find(target.get()) != m_hitOnce.end())
		{
			return;
		}
		m_hitOnce.insert(target.get());

		DamageInfo info;
		int dmg = m_explosionDamage;
		if (auto dd = GetComponent<DamageDealer>(false))
		{
			dmg = dd->GetDamage();
		}
		info.m_Damage = GameDebugSettingsStore::ApplyPlayerDamageMultiplier(dmg);
		if (info.m_Damage <= 0)
		{
			return;
		}
		info.m_Instigator = GetThis<GameObject>();
		info.m_DelayDeathUntilLanding = true;

		if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(target))
		{
			const bool defeatedByThisExplosion = enemyProxy->ApplyDamage(info);

			// ヒットストップ
			auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
			if (gameStage)
			{
				gameStage->RequestHitStop(0.1, 0.03);
				if (defeatedByThisExplosion)
				{
					// 同じ爆弾の累計撃破数が伸びるたび、ステージ側の最高記録を更新する。
					++m_explosionKillCount;
					gameStage->RecordExplosionKills(m_explosionKillCount);
				}
			}

			if (!enemyProxy->IsAlive())
			{
				return;
			}

			auto bombTrans = GetComponent<Transform>();
			auto targetTrans = target->GetComponent<Transform>();
			Vec3 knockbackVelocity(0.0f, 10.0f, 0.0f);
			if (bombTrans && targetTrans)
			{
				Vec3 explosionCenter = bombTrans->GetPosition();
				Vec3 targetPos = targetTrans->GetPosition();
				Vec3 knockbackDir = targetPos - explosionCenter;
				knockbackDir.y = 0.0f;
				float distance = knockbackDir.length();
				if (distance <= 0.01f)
				{
					knockbackDir = Vec3(0.0f, 0.0f, 1.0f);
					distance = 0.0f;
				}

				knockbackDir.normalize();
				float maxKnockbackDist = m_explosionScale * 0.5f;
				float strength = 1.15f - bsmUtil::Min(distance / maxKnockbackDist, 1.0f);
				strength = bsmUtil::Max(strength, 0.45f);
				knockbackVelocity = knockbackDir * (10.0f * strength);
				knockbackVelocity.y = 18.0f * strength;
			}
			// Transformを取得できない場合も、致死敵が離陸待ちのまま残らないよう上向き速度は必ず与える。
			enemyProxy->AddKnockback(knockbackVelocity);
			return;
		}

		info.m_DelayDeathUntilLanding = false;

		// ダメージ適用
		if (auto hp = target->GetComponent<Health>(false))
		{
			hp->ApplyDamage(info);
		}

		// ターゲットが死んでいたら吹き飛ばしも不要
		if (auto hp = target->GetComponent<Health>(false))
		{
			if (hp->IsDead())
			{
				return;
			}
		}

		// --- 吹き飛ばし処理（Gravityコンポーネントを利用） ---
		if (auto gravity = target->GetComponent<Gravity>(false))
		{
			// 爆発中心から対象への方向を計算
			auto bombTrans = GetComponent<Transform>();
			auto targetTrans = target->GetComponent<Transform>();
			if (bombTrans && targetTrans)
			{
				Vec3 explosionCenter = bombTrans->GetPosition();
				Vec3 targetPos = targetTrans->GetPosition();
				Vec3 knockbackDir = targetPos - explosionCenter;
				
				// 水平方向の吹き飛ばし
				knockbackDir.y = 0.0f;
				float distance = knockbackDir.length();
				
				if (distance > 0.01f)
				{
					knockbackDir.normalize();
					
					// 距離に応じて威力を調整（近いほど強い）
					float maxKnockbackDist = m_explosionScale * 0.5f; // 爆発半径
					float strength = 1.0f - bsmUtil::Min(distance / maxKnockbackDist, 1.0f);
					strength = bsmUtil::Max(strength, 0.3f);
					
					// 吹き飛ばしベクトル（水平方向 + 上方向）
					Vec3 knockbackVelocity = knockbackDir * (15.0f * strength); // 水平方向の速度
					knockbackVelocity.y = 20.0f * strength; // 上方向の速度
					
					// Gravityコンポーネントの速度を上書き（既存のジャンプ機能を流用）
					gravity->SetGravityVelocity(knockbackVelocity);
				}
			}
		}
	}

	bool BombBullet::TryGetStageGroundHeight(const Vec3& position, float& outHeight) const noexcept
	{
		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (!gameStage)
		{
			return false;
		}

		return gameStage->TryGetSlopeGroundHeight(position, outHeight);
	}

	Vec3 BombBullet::SnapTargetToStageGround(const Vec3& target) const noexcept
	{
		Vec3 snapped = target;
		float groundY = 0.0f;
		if (TryGetStageGroundHeight(target, groundY))
		{
			snapped.y = groundY;
		}
		return snapped;
	}

	bool BombBullet::TryResolveTerrainImpact(
		const Vec3& previousPosition,
		const Vec3& currentPosition,
		Vec3& outImpactPosition) const noexcept
	{
		float currentGroundY = 0.0f;
		if (!TryGetStageGroundHeight(currentPosition, currentGroundY))
		{
			return false;
		}

		float previousGroundY = currentGroundY;
		TryGetStageGroundHeight(previousPosition, previousGroundY);

		const float previousClearance = previousPosition.y - previousGroundY;
		const float currentClearance = currentPosition.y - currentGroundY;
		const float impactTolerance = 0.08f;
		if (currentClearance > impactTolerance)
		{
			return false;
		}
		if (previousClearance <= currentClearance && currentClearance > -impactTolerance)
		{
			return false;
		}

		const float denom = previousClearance - currentClearance;
		float t = denom > 1e-5f ? previousClearance / denom : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		outImpactPosition = previousPosition + ((currentPosition - previousPosition) * t);
		float impactGroundY = currentGroundY;
		TryGetStageGroundHeight(outImpactPosition, impactGroundY);
		outImpactPosition.y = impactGroundY;
		return true;
	}

	bool BombBullet::SolveBallisticApexHeight(
		const Vec3& p0, const Vec3& p1,
		const Vec3& gravity, float arcHeight,
		Vec3& outV0, float& outT) const
	{
		const float g = -gravity.y;
		if (g <= 1e-6f) return false;

		const float apexY = bsmUtil::Max(p0.y, p1.y) + arcHeight;

		const float h0 = bsmUtil::Max(0.0f, apexY - p0.y);
		const float h1 = bsmUtil::Max(0.0f, apexY - p1.y);

		const float vY0 = std::sqrt(2.0f * g * h0);
		const float tUp = vY0 / g;
		const float tDown = std::sqrt(2.0f * h1 / g);
		const float totalTime = bsmUtil::Max(0.001f, tUp + tDown);

		const Vec3 deltaXZ(p1.x - p0.x, 0.0f, p1.z - p0.z);
		const Vec3 vXZ = deltaXZ * (1.0f / totalTime);

		outV0 = Vec3(vXZ.x, vY0, vXZ.z);
		outT = totalTime;
		return true;
	}

	void BombBullet::ResetForSpawn() noexcept
	{
		DefaultBullet::ResetForSpawn();

		m_fuseTime = m_defaultFuseTime;
		m_exploding = false;
		m_explosionTimer = 0.0;
		m_hitOnce.clear();
		m_explosionKillCount = 0;

		m_flyTime = 0.0f;
		m_totalT = 0.0f;
		m_useBallistic = false;
		m_useGeneratedGroundImpact = true;

		auto trans = GetComponent<Transform>();
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
			return;
		}

		m_useGeneratedGroundImpact = !m_hasTargetSurface || m_targetNormal.y > 0.45f;
		if (m_useGeneratedGroundImpact)
		{
			m_targetPos = SnapTargetToStageGround(m_targetPos);
		}

		const Vec3 deltaXZ(m_targetPos.x - m_startPos.x, 0.0f, m_targetPos.z - m_startPos.z);
		const float distXZ = deltaXZ.length();
		const float arcHeight = m_arcHeight + distXZ * m_arcHeightPerDistXZ;

		// ターゲット弾道（プレビューと同一計算）
		Vec3 v0;
		float totalTime = 0.0f;
		if (SolveBallisticApexHeight(m_startPos, m_targetPos, m_gravity, arcHeight, v0, totalTime))
		{
			m_v0 = v0;
			m_totalT = totalTime;
			m_useBallistic = true;

			// 初速（向き回転などに使うなら）
			m_velocity = m_v0;

			// ターゲットに到達する前に信管が切れないよう信管
			// （着弾に爆発させないなら、この信管は事実上不要）
			m_fuseTime = bsmUtil::Max(m_fuseTime, static_cast<double>(totalTime) + 0.2);
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
	}
}
