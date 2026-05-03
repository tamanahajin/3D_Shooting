/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {


	SkyDome::SkyDome(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	SkyDome::~SkyDome() {}

	void SkyDome::OnCreate()
	{
		SetShadowActive(false);
		SetAlphaActive(false);

		auto ptrDraw = AddComponent<SkyDomeDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetRadius(450.0f);

		AddTag(L"Sky");
	}

	//--------------------------------------------------------------------------------------
	// フロアオブジェクト
	//--------------------------------------------------------------------------------------
	Floor::Floor(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix)
	{
		m_transParam = param;
	}
	Floor::~Floor() {}

	void Floor::OnCreate()
	{
		AddTag(L"Floor");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		ptrDraw->AddBaseModelMesh(meshes);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(
				m_MaterialPrefix + std::to_wstring(i)
			);
		}

		ptrDraw->SetOwnShadowActive(false);
	}

	FloorInstancedRenderer::FloorInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix),
		m_InstanceWorlds(instanceWorlds)
	{
	}

	FloorInstancedRenderer::~FloorInstancedRenderer() {}

	void FloorInstancedRenderer::OnCreate()
	{
		auto ptrDraw = AddComponent<InstancedStaticDraw>();

		ptrDraw->SetMeshKey(m_MeshKey);
		ptrDraw->SetMaterialPrefix(m_MaterialPrefix);
		ptrDraw->SetInstanceWorlds(m_InstanceWorlds);
		ptrDraw->SetBaseColorOverride(Col4(0.627f, 0.659f, 0.788f, 1.0f));
		ptrDraw->SetUseMaterialTexture(false);
		ptrDraw->SetLightingEnabled(true);
		ptrDraw->SetOwnShadowActive(false);
		ptrDraw->BuildInstanceBuffer();

		AddTag(L"Floor");
	}


	FloorCollision::FloorCollision(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const Vec3& collisionSize) :
		GameObject(stage),
		m_CollisionSize(collisionSize)
	{
		m_transParam = param;
	}

	FloorCollision::~FloorCollision() {}

	void FloorCollision::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_CollisionSize.x,
			m_CollisionSize.y,
			m_CollisionSize.z);
		ptrColl->SetFixed(true);

		AddTag(L"Floor");
	}

	//--------------------------------------------------------------------------------------
	// ボックスオブジェクト
	//--------------------------------------------------------------------------------------
	FixedBox::FixedBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage)
	{
		m_transParam = param;
	}
	FixedBox::~FixedBox() {}

	void FixedBox::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetFixed(true);
		//タグをつける
		AddTag(L"FixedBox");
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	//--------------------------------------------------------------------------------------
	// 四角のオブジェクト
	//--------------------------------------------------------------------------------------
	WallBox::WallBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage),
		m_totalTime(0.0)
	{
		m_transParam = param;
	}
	WallBox::~WallBox() {}

	void WallBox::OnCreate()
	{
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		//重力をつける
		auto ptrGra = AddComponent<Gravity>();

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	void WallBox::OnUpdate(double elapsedTime)
	{
		//Transformコンポーネントを取り出す
		auto ptrTrans = GetComponent<Transform>();
		auto& param = ptrTrans->GetTransParam();

		m_totalTime += elapsedTime;
		if (m_totalTime >= XM_2PI)
		{
			m_totalTime = 0.0;
		}
		param.position.x = (float)sin(m_totalTime) * 2.0f;
	}


	//--------------------------------------------------------------------------------------
	//	追いかける配置オブジェクト
	//--------------------------------------------------------------------------------------
	//構築と破棄
}
