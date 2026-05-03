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

		m_IsActive = false;
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
		if (!m_IsActive) return;

		// 寿命
		m_ElapsedTime += elapsedTime;
		if (m_ElapsedTime >= m_LifeTime)
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
			param.position += forward * m_Speed * static_cast<float>(elapsedTime);
		}
	}

	bool DefaultBullet::IsActive() const noexcept
	{
		return m_IsActive;
	}

	void DefaultBullet::SetActive(bool active) noexcept
	{
		m_IsActive = active;
	}

	void DefaultBullet::OnCollisionEnter(const CollisionPair& pair)
	{
		if (!m_IsActive) return;

		auto other = pair.m_Dest.lock();
		if (!other) return;

		auto otherObj = other->GetGameObject();
		if (!otherObj) return;

		// プレイヤーは除外
		if (otherObj->FindTag(L"Player")) return;

		DamageInfo info;
		info.m_Damage = GetComponent<DamageDealer>()->GetDamage();
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
		dmg->SetDamage(m_ExplosionDamage);
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

		if (!m_Exploding)
		{
			// 飛行中（信管）
			m_FuseTime -= elapsedTime;
			if (m_FuseTime <= 0.0)
			{
				StartExplosion(nullptr);
				return;
			}

			auto trans = GetComponent<Transform>();
			if (!trans) return;

			auto& tp = trans->GetTransParam();

			if (m_UseBallistic)
			{
				// 発射からの経過 t
				m_FlyTime += static_cast<float>(elapsedTime);

				// 弾道式：p(t)=p0+v0*t+0.5*g*t^2
				const float t = m_FlyTime;
				tp.position = m_StartPos + (m_V0 * t) + (m_Gravity * (0.5f * t * t));

				// 現在速度：v(t)=v0+g*t（必要なら）
				m_Velocity = m_V0 + (m_Gravity * t);

				// --- 重要： 「プレビュー終点」 と一致させるコツ ---
				// プレビューは 0..T で描画して終了なので、弾側も T で終了または着弾させる
				if (m_FlyTime >= m_TotalT)
				{
					tp.position = m_TargetPos;   // 最後ぴったりと合わせる
					StartExplosion(nullptr);     // 着弾で爆発（不要ならコメントアウトして停止など）
					return;
				}
			}
			else
			{
				// ターゲットなし：従来の直進（+重力）でもOK
				const float dt = static_cast<float>(elapsedTime);
				m_Velocity += m_Gravity * dt;
				tp.position += m_Velocity * dt;
			}
		}
		else
		{
			m_ExplosionTimer -= elapsedTime;
			if (m_ExplosionTimer <= 0.0)
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

		if (!m_Exploding)
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
		if (m_Exploding)
		{
			OnCollisionEnter(pair);
		}
	}

	void BombBullet::StartExplosion(const std::shared_ptr<GameObject>& firstHit)
	{
		if (m_Exploding) return;

		m_Exploding = true;
		m_ExplosionTimer = m_ExplosionDuration;
		m_Velocity = Vec3(0, 0, 0);
		m_HitOnce.clear();

		Vec3 explosionPos(0.0f, 0.0f, 0.0f);

		// 爆風範囲を「スケール拡大」で表現
		if (auto trans = GetComponent<Transform>())
		{
			explosionPos = trans->GetPosition();
			trans->SetScale(Vec3(m_ExplosionScale, m_ExplosionScale, m_ExplosionScale));
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
			fx->SetScaleRange(0.15f, bsmUtil::Max(1.4f, m_ExplosionScale * 0.9f));
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
		if (m_HitOnce.find(target.get()) != m_HitOnce.end())
		{
			return;
		}
		m_HitOnce.insert(target.get());

		DamageInfo info;
		int dmg = m_ExplosionDamage;
		if (auto dd = GetComponent<DamageDealer>(false))
		{
			dmg = dd->GetDamage();
		}
		info.m_Damage = dmg;
		info.m_Instigator = GetThis<GameObject>();

		if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(target))
		{
			enemyProxy->ApplyDamage(info);
			if (!enemyProxy->IsAlive())
			{
				return;
			}

			auto bombTrans = GetComponent<Transform>();
			auto targetTrans = target->GetComponent<Transform>();
			if (bombTrans && targetTrans)
			{
				Vec3 explosionCenter = bombTrans->GetPosition();
				Vec3 targetPos = targetTrans->GetPosition();
				Vec3 knockbackDir = targetPos - explosionCenter;
				knockbackDir.y = 0.0f;
				float distance = knockbackDir.length();
				if (distance > 0.01f)
				{
					knockbackDir.normalize();
					float maxKnockbackDist = m_ExplosionScale * 0.5f;
					float strength = 1.15f - bsmUtil::Min(distance / maxKnockbackDist, 1.0f);
					strength = bsmUtil::Max(strength, 0.45f);
					Vec3 knockbackVelocity = knockbackDir * (10.0f * strength);
					knockbackVelocity.y = 18.0f * strength;
					enemyProxy->AddKnockback(knockbackVelocity);
				}
			}
			return;
		}

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
					float maxKnockbackDist = m_ExplosionScale * 0.5f; // 爆発半径
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

	bool BombBullet::SolveBallistic_ApexHeight(
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
		const float T = bsmUtil::Max(0.001f, tUp + tDown);

		const Vec3 deltaXZ(p1.x - p0.x, 0.0f, p1.z - p0.z);
		const Vec3 vXZ = deltaXZ * (1.0f / T);

		outV0 = Vec3(vXZ.x, vY0, vXZ.z);
		outT = T;
		return true;
	}

	void BombBullet::ResetForSpawn() noexcept
	{
		DefaultBullet::ResetForSpawn();

		m_FuseTime = FUSE_TIME;
		m_Exploding = false;
		m_ExplosionTimer = 0.0;
		m_HitOnce.clear();

		m_FlyTime = 0.0f;
		m_TotalT = 0.0f;
		m_UseBallistic = false;

		auto trans = GetComponent<Transform>();
		if (!trans)
		{
			m_HasTarget = false;
			return;
		}

		// 発射時の起点 p0
		m_StartPos = trans->GetTransParam().position;

		// ターゲットなければ信管で直進
		if (!m_HasTarget)
		{
			m_V0 = trans->GetForward() * m_Speed;
			m_Velocity = m_V0;
			return;
		}

		const Vec3 deltaXZ(m_TargetPos.x - m_StartPos.x, 0.0f, m_TargetPos.z - m_StartPos.z);
		const float distXZ = deltaXZ.length();
		const float arcHeight = m_ArcHeight + distXZ * m_ArcHeightPerDistXZ;

		// ターゲット弾道（プレビューと同一計算）
		Vec3 v0;
		float T = 0.0f;
		if (SolveBallistic_ApexHeight(m_StartPos, m_TargetPos, m_Gravity, arcHeight, v0, T))
		{
			m_V0 = v0;
			m_TotalT = T;
			m_UseBallistic = true;

			// 初速（向き回転などに使うなら）
			m_Velocity = m_V0;

			// ターゲットに到達する前に信管が切れないよう信管
			// （着弾に爆発させないなら、この信管は事実上不要）
			m_FuseTime = bsmUtil::Max(m_FuseTime, (double)T + 0.2);
		}
		else
		{
			// 解けなかったら従来の直進にフォールバック
			m_V0 = trans->GetForward() * m_Speed;
			m_Velocity = m_V0;
			m_UseBallistic = false;
		}

		// 繰り返し時の防止
		m_HasTarget = false;
	}
}

