#include "stdafx.h"
#include "Project.h"

namespace shooting {

	DefaultBullet::DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
		: IBullet(stagePtr)
		, m_Speed(6.0f)
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
		//SetUpdateActive(false);
		//if (auto trans = GetComponent<Transform>())
		//{
		//	trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
		//}
	}

	void DefaultBullet::OnCollisionExecute(const CollisionPair& pair)
	{
		//OnCollisionEnter(pair);
	}
}