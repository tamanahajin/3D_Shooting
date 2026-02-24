#include "stdafx.h"
#include "Project.h"

namespace shooting {

	//============================================================
	// DefaultBullet
	//============================================================
	DefaultBullet::DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: IBullet(stagePtr)
	{
		// ObjectFactory::Create はコンストラクタ直後に OnCreate() を呼ぶので
		// Transform初期値を先に持たせたい場合はここで保存する
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

		// ダメージ（このプロジェクトでは OnCollisionEnter 内で ApplyDamage している）
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

		// 相手がHPを持っていればダメージ
		if (auto hp = otherObj->GetComponent<Health>(false))
		{
			DamageInfo info;
			info.m_Damage = GetComponent<DamageDealer>()->GetDamage();
			info.m_Instigator = GetThis<GameObject>();
			hp->ApplyDamage(info);
		}

		// 命中で終了（プール回収は BulletPool 側）
		SetActive(false);
		SetUpdateActive(false);
	}

	void DefaultBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 必要なら Enter と同様に扱う
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
		// まず通常弾の部品をそのまま使う（衝突・描画・DamageDealer 等）
		DefaultBullet::OnCreate();

		// ボム用タグ（デバッグ用）
		AddTag(L"Bomb");

		// ボムは「爆発中に複数ヒット」させたいので DestroyOnHit は false 推奨
		if (auto dd = GetComponent<DamageDealer>(false))
		{
			dd->SetDamage(m_ExplosionDamage);
			dd->SetDestroyOnHit(false);
		}
	}

	void BombBullet::OnUpdate(double elapsedTime)
	{
		if (!IsActive()) return;

		if (!m_Exploding)
		{
			// 信管（保険）
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

				// 解析式：p(t)=p0+v0*t+0.5*g*t^2
				const float t = m_FlyTime;
				tp.position = m_StartPos + (m_V0 * t) + (m_Gravity * (0.5f * t * t));

				// 現在速度：v(t)=v0+g*t（必要なら）
				m_Velocity = m_V0 + (m_Gravity * t);

				// --- ここが “プレビュー終点” と一致させるコツ ---
				// プレビューは 0..T を描いて終わりなので、実弾も T で終わらせる（=到達で爆発）
				if (m_FlyTime >= m_TotalT)
				{
					tp.position = m_TargetPos;   // 最後をピタッと合わせる
					StartExplosion(nullptr);     // 到達で爆発（不要ならコメントアウトして停止などに変更）
					return;
				}
			}
			else
			{
				// ターゲット無し等：従来通り（直進+重力）でもOK
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
			// 飛翔中に何かに当たったら即爆発
			StartExplosion(otherObj);
			return;
		}

		// 爆発中は範囲ダメージ（多重ヒット防止あり）
		TryApplyExplosionDamage(otherObj);
	}

	void BombBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 爆風が短い場合 Enter だけで十分なことが多い。
		// もし衝突イベントの仕様で Execute の方が確実ならここで処理する。
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
		// ヒット記録クリア
		m_HitOnce.clear();

		// 爆風範囲を「スケール拡大」で表現
		if (auto trans = GetComponent<Transform>())
		{
			trans->SetScale(Vec3(m_ExplosionScale, m_ExplosionScale, m_ExplosionScale));
		}

		// 着弾先があるなら即ダメージ（爆発開始時点で衝突中の可能性があるため）
		if (firstHit)
		{
			TryApplyExplosionDamage(firstHit);
		}

		// ここで爆発VFX/SE/カメラシェイク等を入れるのが定番
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

		if (auto hp = target->GetComponent<Health>(false))
		{
			DamageInfo info;
			int dmg = m_ExplosionDamage;
			if (auto dd = GetComponent<DamageDealer>(false))
			{
				dmg = dd->GetDamage();
			}

			info.m_Damage = dmg;
			info.m_Instigator = GetThis<GameObject>();
			hp->ApplyDamage(info);
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

		// 発射時の基準点 p0
		m_StartPos = trans->GetTransParam().position;

		// ターゲットが無い場合は保険で直進
		if (!m_HasTarget)
		{
			m_V0 = trans->GetForward() * m_Speed;
			m_Velocity = m_V0;
			return;
		}

		const Vec3 deltaXZ(m_TargetPos.x - m_StartPos.x, 0.0f, m_TargetPos.z - m_StartPos.z);
		const float distXZ = deltaXZ.length();
		const float arcHeight = m_ArcHeight + distXZ * m_ArcHeightPerDistXZ;

		// ターゲット弾道（プレビューと同じ解き方）
		Vec3 v0;
		float T = 0.0f;
		if (SolveBallistic_ApexHeight(m_StartPos, m_TargetPos, m_Gravity, arcHeight, v0, T))
		{
			m_V0 = v0;
			m_TotalT = T;
			m_UseBallistic = true;

			// 今の速度（見た目回転などに使うなら）
			m_Velocity = m_V0;

			// ターゲットに到達する前に信管爆発しないよう保険
			// （到達時に爆発させるなら、この保険は実質不要）
			m_FuseTime = bsmUtil::Max(m_FuseTime, (double)T + 0.2);
		}
		else
		{
			// 解けなければ直進にフォールバック
			m_V0 = trans->GetForward() * m_Speed;
			m_Velocity = m_V0;
			m_UseBallistic = false;
		}

		// 使い回し事故防止
		m_HasTarget = false;
	}
}
