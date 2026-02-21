#include "stdafx.h"
#include "Project.h"

namespace shooting {

	DefaultBullet::DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: IBullet(stagePtr)
		, m_Speed(15.0f)
		, m_IsActive(true)
		, m_LifeTime(5.0)
		, m_ElapsedTime(0.0)
	{
		// ★重要：ObjectFactory::Create はコンストラクタ直後に OnCreate() を呼ぶので、
		// OnCreate の前に初期Transformを渡したいならここで保持しておく
		m_transParam = param; // Player と同じ方式（GameObject側にある想定）
	}

	void DefaultBullet::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		//OBB衝突判定を付ける
		auto ptrColl = AddComponent<CollisionSphere>();
		ptrColl->SetFixed(false);
		//タグをつける
		AddTag(L"Bullet");
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_SPHERE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		ptrDraw->SetOwnShadowActive(true);

		auto dmg = AddComponent<DamageDealer>();
		dmg->SetDamage(3);
		dmg->SetDestroyOnHit(true);

		if (auto col = GetComponent<Collision>(false))
		{
			col->AddExcludeCollisionTag(L"Player");
		}
	}

	void DefaultBullet::OnUpdate(double elapsedTime)
	{
		// 寿命の計算
		m_ElapsedTime += elapsedTime;
		if (m_ElapsedTime >= m_LifeTime)
		{
			SetActive(false);
			return;
		}

		// 進行方向に移動
		auto ptrTrans = GetComponent<Transform>();
		auto& param = ptrTrans->GetTransParam();
		Vec3 forward = ptrTrans->GetForward();
		param.position += forward * m_Speed * static_cast<float>(elapsedTime);
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

		// --- 例：プレイヤーに当たったら無視したい場合（任意） ---
		// 相手オブジェクトを取れるなら、タグで弾消し対象を絞るのが安全
		// auto other = pair.m_DstObject;  // ←ここは CollisionPair に合わせて
		// if (other && other->FindTag(L"Player")) return;

		auto other = pair.m_Dest.lock();
		if(!other) return;

		auto otherObj = other->GetGameObject();
		if (!otherObj) return;

		if (otherObj->FindTag(L"Player")) return;

		// 相手がHPを持っているならダメージ
		if (auto hp = otherObj->GetComponent<Health>(false))
		{
			DamageInfo info;
			info.m_Damage = GetComponent<DamageDealer>()->GetDamage();
			info.m_Instigator = GetThis<GameObject>();
			// info.hitPoint = pair...;
			// info.hitNormal = pair...;

			hp->ApplyDamage(info);
		}

		// 当たったら消す
		SetActive(false);

		// すぐ見えなくしたいなら（次フレームのプール回収を待たない）
		SetUpdateActive(false);
		if (auto trans = GetComponent<Transform>())
		{
			trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
		}
	}

	void DefaultBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		//OnCollisionEnter(pair);
	}

	BombBullet::BombBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: DefaultBullet(stagePtr, param)
	{
		// ResetLife() はプール側が呼ぶ（DefaultBulletの機構を利用）
		// ここでは爆弾特有の初期値だけ持つ
	}

	void BombBullet::OnCreate()
	{
		// まず DefaultBullet と同じ構成（CollisionSphere / Draw / DamageDealer / Exclude Player）を作る
		DefaultBullet::OnCreate();

		// タグを追加しておくとデバッグに便利
		AddTag(L"Bomb");

		// DamageDealer を「爆発用」に更新（単発ヒットで消すのは自前でやるので false）
		if (auto dd = GetComponent<DamageDealer>(false))
		{
			dd->SetDamage(m_ExplosionDamage);
			dd->SetDestroyOnHit(false);
		}

		// ここで見た目のメッシュ/テクスチャを差し替えたいなら
		// BcPNTStaticDraw を取得して設定変更してOK
		// (例) bomb用のテクスチャがあるなら AddBaseTexture など
	}

	void BombBullet::OnUpdate(double elapsedTime)
	{
		if (!IsActive()) return;

		if (!m_Exploding)
		{
			//============================
			// Flying：移動 + 信管
			//============================
			m_FuseTime -= elapsedTime;
			if (m_FuseTime <= 0.0)
			{
				// 何にも当たらなくても爆発（firstHitは無し）
				StartExplosion(nullptr);
				return;
			}

			// 前進（DefaultBulletのprivate速度に触れないため、こちらで移動を実装）
			if (auto trans = GetComponent<Transform>())
			{
				auto& tp = trans->GetTransParam();
				Vec3 forward = trans->GetForward();
				tp.position += forward * m_Speed * static_cast<float>(elapsedTime);
			}
		}
		else
		{
			//============================
			// Exploding：短時間だけ範囲当たり判定を維持
			//============================
			m_ExplosionTimer -= elapsedTime;
			if (m_ExplosionTimer <= 0.0)
			{
				// 爆発終了 → 弾をプールへ戻す
				SetActive(false);
				SetUpdateActive(false);

				// 画面外へ退避（プールがやってるのと同じ）
				if (auto trans = GetComponent<Transform>())
				{
					trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
					// スケールも戻す（次回スポーン時に違和感が出ないように）
					trans->SetScale(Vec3(1.0f, 1.0f, 1.0f));
				}
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

		// 自分（プレイヤー）には当てない
		if (otherObj->FindTag(L"Player")) return;

		// 飛翔中に当たったら即爆発
		if (!m_Exploding)
		{
			StartExplosion(otherObj);
			return;
		}

		// 爆発中は、触れた相手へ範囲ダメージ（多重ヒット防止付き）
		TryApplyExplosionDamage(otherObj);
	}

	void BombBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		// 爆風は一瞬なので、Enterだけでも充分なことが多い。
		// ただし、Collisionシステムの仕様によっては Execute の方が確実な場合もある。
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

		// 爆発開始時の信管値は無意味になるので、適当に0へ
		m_FuseTime = 0.0;

		// ヒット履歴をクリア
		m_HitOnce.clear();

		// 「爆風」を当たり判定として表現する：
		// CollisionSphere が Transform のスケールに追従する前提で、スケールを一気に大きくする。
		// もし追従しない場合は、CollisionSphere 側の SetRadius/SetScale など適切なAPIに置き換えてください。
		if (auto trans = GetComponent<Transform>())
		{
			trans->SetScale(Vec3(m_ExplosionScale, m_ExplosionScale, m_ExplosionScale));
		}

		// 着弾した相手がいるなら、開始時点で必ずダメージを入れる
		// （爆発開始時はすでに衝突中なので、Enterが再発しない可能性があるため）
		if (firstHit)
		{
			TryApplyExplosionDamage(firstHit);
		}

		// ここで「爆発エフェクト」「サウンド」「カメラシェイク」等を鳴らすのが定番
		// 例：GetStage()->AddGameObject<ExplosionVFX>(...) など
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

		// HPを持つ相手にダメージ
		if (auto hp = target->GetComponent<Health>(false))
		{
			DamageInfo info;
			// DamageDealerがあるならそれを優先、無ければ m_ExplosionDamage
			int dmg = m_ExplosionDamage;
			if (auto dd = GetComponent<DamageDealer>(false))
			{
				dmg = dd->GetDamage();
			}

			info.m_Damage = dmg;
			info.m_Instigator = GetThis<GameObject>();
			// info.m_HitPoint / m_HitNormal は取れるなら pair から設定するとより良い

			hp->ApplyDamage(info);
		}
	}
}