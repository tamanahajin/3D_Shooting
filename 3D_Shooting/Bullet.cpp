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
}